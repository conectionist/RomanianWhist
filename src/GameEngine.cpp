#include <romanian_whist/GameEngine.h>

#include <romanian_whist/CardValidator.h>
#include <romanian_whist/IGameObserver.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace romanian_whist
{
GameEngine::GameEngine() : status(GameStatus::NotStarted), generator(std::random_device{}())
{}

GameEngine::GameEngine(std::uint32_t seed) : status(GameStatus::NotStarted), generator(seed)
{}

void GameEngine::addPlayer(const std::string &name, std::unique_ptr<IMoveProvider> moveProvider)
{
    // Once the deck is built, its size is fixed to the player count at that
    // moment - a seat added afterwards would leave dealCards() indexing
    // past the end of it (initializeDeck()'s own guard only checks at the
    // point it runs, not retroactively).
    if(deckInitialized)
        throw std::logic_error("cannot add a player after initializeDeck() has been called");

    // The round schedule is sized and its opener rotation laid out for the
    // player count at the moment initializeScoreboard() runs. A seat added
    // afterwards leaves the game playing, say, a 3-player 21-round schedule
    // with 4 seats, and the rotation no longer returning to the same opener.
    if(scoreboardInitialized)
        throw std::logic_error("cannot add a player after initializeScoreboard() has been called");

    // Bets and tricks are keyed by Seat now, so a shared name no longer
    // corrupts scoring. It is still rejected because a name is how a client
    // labels a seat on screen and how a player identifies their own: two "Ana"
    // rows that cannot be told apart is a bad game, just no longer a wrong one.
    for(const auto& player : players)
    {
        if(player.getName() == name)
            throw std::invalid_argument("duplicate player name: " + name);
    }

    players.addPlayer(name, std::move(moveProvider));
}

void GameEngine::initializeScoreboard(const GameStructure &structure, 
                                      bool endWithForeheadAndHidden, 
                                      bool all1GamesAreForehead)
{
    // Scoreboard::initialize() appends its rounds without clearing what is
    // already there, so a second call would leave a 21-round schedule 42
    // rounds long, with the opener rotation restarting halfway through.
    // Reject it outright, as initializeDeck() does.
    if(scoreboardInitialized)
        throw std::logic_error("initializeScoreboard() has already been called");

    scoreboard.initialize(structure, endWithForeheadAndHidden, all1GamesAreForehead, players);

    scoreboardInitialized = true;
}

void GameEngine::initializeDeck(unsigned int playerCount)
{
    if(playerCount < 2 || playerCount > 6)
        throw std::invalid_argument("playerCount must be between 2 and 6");

    // dealCards() indexes the deck by players.size(), not by this argument -
    // a mismatch here builds a deck sized for the wrong player count and
    // dealCards() then reads past the end of it.
    if(playerCount != players.size())
        throw std::invalid_argument("playerCount must match the number of players added");

    // Deck::addCard only ever appends, and by the time a deal has happened
    // every dealt Card* points into this exact buffer - rebuilding it would
    // dangle every one of them rather than just duplicate cards. Reject a
    // second call outright instead of trying to make it safe.
    if(deckInitialized)
        throw std::logic_error("initializeDeck() has already been called");

    for(int s = 0 ; s < 4 ; s++)
        for(int r = 1 + (6 - playerCount) * 2 ; r < 13 ; r++)
        {
            Card card(static_cast<Rank>(r), static_cast<Suit>(s));
            deck.addCard(std::move(card));
        }

    deckInitialized = true;
}

void GameEngine::setStatus(GameStatus _status)
{
    // Moving off NotStarted for the first time is the start of the game. Phase
    // 3 gives that transition its own method (start()) and this notification
    // moves there; until then this is the one place it can honestly happen,
    // since run() may be called more than once and would either duplicate the
    // event or misplace it onto the game thread.
    const bool starting = status == GameStatus::NotStarted && _status == GameStatus::InProgress;

    status = _status;

    if(starting)
        notifyGameStarted();
}

GameStatus GameEngine::getStatus() const
{
    return status;
}

bool GameEngine::isInProgress() const
{
    return status == GameStatus::InProgress;
}

GamePhase GameEngine::getPhase() const
{
    return phase;
}

void GameEngine::addObserver(IGameObserver* observer)
{
    if(observer == nullptr)
        throw std::invalid_argument("GameEngine::addObserver: observer must not be null");

    if(std::find(observers.begin(), observers.end(), observer) != observers.end())
        return;

    observers.push_back(observer);
}

void GameEngine::removeObserver(IGameObserver* observer)
{
    observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
}

void GameEngine::run()
{
    requireInProgress();

    while(isInProgress())
    {
        if(honourStopIfRequested())
            return;

        playRound();
    }

    // onGameOver() and onGameStopped() are fired by playRound() and
    // honourStopIfRequested(), at the point the transition actually happens -
    // so a client driving playRound() itself is told the same things in the
    // same order as one calling run().
}

void GameEngine::playRound()
{
    requireInProgress();

    dealRound();
    notifyRoundStarted();

    runBidding();
    notifyBettingComplete();

    const unsigned int trickCount = getCurrentRoundTrickCount();

    for(unsigned int trickNumber = 1 ; trickNumber <= trickCount ; trickNumber++)
    {
        // The boundary a stop lands on. Checked here rather than mid-turn, so a
        // trick already under way always finishes.
        if(honourStopIfRequested())
            return;

        Round& round = scoreboard.getCurrentRound();

        currentTrickNumber = trickNumber;
        round.resetCurrentTrick();
        notifyTrickStarted(trickNumber, round.getTrickLeaderSeat());

        playTrick();

        const Seat winner = determineTrickWinner(round.getCurrentTrick());

        // Filed before observers are told, so getTricksWon(winner) already
        // counts this trick inside onTrickWon() - and deliberately not cleared,
        // so getCurrentTrick() is still the finished trick there. The next
        // resetCurrentTrick() above is what clears it.
        round.finishCurrentTrick(winner);
        round.setTrickLeaderSeat(winner);

        activeSeat.reset();
        notifyTrickWon(winner, trickNumber);
    }

    // Scored but not committed, so an observer can show what the round was
    // worth alongside the total it is about to fold into.
    calculateScores();
    notifyRoundScored();

    commitRoundScores();
    completeCurrentRound();
    notifyRoundComplete();

    if(status == GameStatus::Finished)
        notifyGameOver();
}

void GameEngine::requestStop()
{
    stopRequested.store(true);
}

void GameEngine::dealRound()
{
    shuffleDeck();
    dealCards();

    // A fresh round starts with an empty table, not with the previous round's
    // last trick still standing in it.
    scoreboard.getCurrentRound().resetCurrentTrick();

    currentTrickNumber = 0;
    activeSeat.reset();
    phase = GamePhase::Betting;
}

void GameEngine::runBidding()
{
    Seat seat = scoreboard.getCurrentRound().getRoundLeaderSeat();

    for(unsigned int i = 0 ; i < players.size() ; i++)
    {
        activeSeat = seat;
        notifyBetRequested(seat);

        Player& player = players.at(seat.index);

        // Asked fresh each time round: getForbiddenBet() only names a value
        // once everyone but the last player has bid.
        const unsigned int bet = player.getBet(getCurrentTrumpCard(), i == 0, getForbiddenBet());

        // The engine is the one asking now, so it is the one that has to judge
        // the answer. Legality used to live entirely in the providers, which is
        // defensible only while every provider ships in this repo - a
        // WebMoveProvider is a thin shim over an untrusted browser.
        if(!isBetLegal(bet))
            throw std::logic_error(player.getName() + " bid " + std::to_string(bet)
                                   + ", which this round does not allow");

        placeBet(seat, bet);
        notifyBetPlaced(seat, bet);

        seat = getNextSeat(seat);
    }

    activeSeat.reset();
    phase = GamePhase::Playing;
}

void GameEngine::playTrick()
{
    Round& round = scoreboard.getCurrentRound();
    const CardValidator validator;

    Seat seat = round.getTrickLeaderSeat();

    for(unsigned int i = 0 ; i < players.size() ; i++)
    {
        activeSeat = seat;
        notifyCardRequested(seat);

        Player& player = players.at(seat.index);
        Card* trump = getCurrentTrumpCard();

        const Trick& trick = round.getCurrentTrick();
        const Suit* leadSuit = trick.hasLeadSuit() ? &trick.getLeadSuit() : nullptr;

        // Worked out before the provider is asked, because Player::playCard
        // erases the chosen card from the hand as part of the same call - after
        // it returns there is no longer a hand to judge the choice against.
        const std::vector<Card*> legalCards = validator.getLegalCards(player.getHand(), trump, leadSuit);

        // The trick so far, plus what this player owes on their bid, so a
        // strategy can tell whether a card would actually win and whether it
        // wants it to.
        Card* playedCard = player.playCard(trump,
                                           leadSuit,
                                           trick.cardsInPlayOrder(),
                                           round.getBet(seat).value_or(0),
                                           round.getTricksWon(seat));

        // Contractually possible: a move provider returns null when it has no
        // legal play. It should not be reachable, since a player holding cards
        // always has at least one legal one - so say so loudly rather than
        // dereferencing null or playing on regardless.
        if(playedCard == nullptr)
            throw std::runtime_error(player.getName() + " had no legal card to play.");

        // Covers both halves of a bad move: a card the player does not hold is
        // not in the legal set either.
        if(std::find(legalCards.begin(), legalCards.end(), playedCard) == legalCards.end())
            throw std::logic_error(player.getName() + " played a card that is not legal in this trick");

        round.addCardToCurrentTrick(seat, playedCard);
        notifyCardPlayed(seat, *playedCard);

        seat = getNextSeat(seat);
    }
}

bool GameEngine::honourStopIfRequested()
{
    if(!stopRequested.load())
        return false;

    // Idempotent: run() and playRound() both check, and a stop must be reported
    // once however many boundaries it crosses.
    if(status == GameStatus::Stopped)
        return true;

    // The phase is deliberately left where it was. A stop lands at a trick
    // boundary with the round unscored, and getStatus() is what says the game
    // is over - getPhase() says where in the round it stopped.
    status = GameStatus::Stopped;
    activeSeat.reset();

    notifyGameStopped();

    return true;
}

void GameEngine::requireInProgress() const
{
    if(status != GameStatus::InProgress)
        throw std::logic_error("GameEngine: the game is not in progress - it has "
                               "not been started, or it has already finished or "
                               "been stopped");
}

void GameEngine::shuffleDeck()
{
    deck.shuffle(generator);
}

void GameEngine::dealCards()
{
    // Without a deck there is nothing to hand out: deck[index] would index an
    // empty Deck and every player would end up holding a Card* into nothing,
    // with the crash deferred to the first dereference far from here.
    if(!deckInitialized)
        throw std::logic_error("dealCards() requires initializeDeck() to have been called");

    // The trick count comes from the current round, and with no schedule
    // there is no current round to read it from.
    if(!scoreboardInitialized)
        throw std::logic_error("dealCards() requires initializeScoreboard() to have been called");

    clearAllPlayerHands();

    unsigned int gameCount = scoreboard.getCurrentRound().getTrickCount();
    unsigned int index = 0;

    for(unsigned int i = 0 ; i < gameCount ; i++)
    {
        for(unsigned int j = 0 ; j < players.size() ; j++)
        {
            index = gameCount * j + i;
            players[j].addCardToHand(&deck[index]);
        }
    }

    if(gameCount < 8)
    {
        scoreboard.getCurrentRound().setTrumpCard(&deck[index + 1]);
    }
}

Card *GameEngine::getCurrentTrumpCard()
{
    requireStarted();

    return scoreboard.getCurrentRound().getTrumpCard();
}

const Card *GameEngine::getCurrentTrumpCard() const
{
    requireStarted();

    return scoreboard.getCurrentRound().getTrumpCard();
}

Seat GameEngine::getTrickLeaderSeat() const
{
    requireStarted();

    return scoreboard.getCurrentRound().getTrickLeaderSeat();
}

Seat GameEngine::getRoundLeaderSeat() const
{
    requireStarted();

    return scoreboard.getCurrentRound().getRoundLeaderSeat();
}

unsigned int GameEngine::getBiddingOrder(Seat seat) const
{
    requireStarted();

    const unsigned int playerCount = players.size();

    if(seat.index >= playerCount)
        throw std::out_of_range("GameEngine::getBiddingOrder: seat out of range");

    const unsigned int roundLeader = getRoundLeaderSeat().index;

    return ((seat.index + playerCount - roundLeader) % playerCount) + 1;
}

Seat GameEngine::getNextSeat(Seat seat) const
{
    return players.nextSeat(seat);
}

unsigned int GameEngine::getPlayerCount() const
{
    return players.size();
}

std::optional<unsigned int> GameEngine::getForbiddenBet() const
{
    requireStarted();

    const Round& round = scoreboard.getCurrentRound();

    unsigned int betsPlaced = 0;
    unsigned int total = 0;

    for(unsigned int i = 0 ; i < players.size() ; i++)
    {
        if(const std::optional<unsigned int> bet = round.getBet(Seat{i}))
        {
            betsPlaced++;
            total += *bet;
        }
    }

    if(betsPlaced + 1 != players.size())
        return std::nullopt;

    const unsigned int trickCount = round.getTrickCount();

    if(total > trickCount)
        return std::nullopt;

    return trickCount - total;
}

bool GameEngine::isBetLegal(unsigned int bet) const
{
    requireStarted();

    if(bet > getCurrentRoundTrickCount())
        return false;

    const std::optional<unsigned int> forbidden = getForbiddenBet();

    return !forbidden || bet != *forbidden;
}

void GameEngine::placeBet(Seat seat, unsigned int bet)
{
    requireStarted();

    scoreboard.getCurrentRound().setBet(seat, bet);
}

void GameEngine::clearAllPlayerHands()
{
    for(auto& player : players)
        player.clearHand();
}

unsigned int GameEngine::getCurrentRoundTrickCount() const
{
    requireStarted();

    return scoreboard.getCurrentRound().getTrickCount();
}

void GameEngine::addTrickToCurrentRound(const Trick& trick)
{
    requireStarted();

    scoreboard.getCurrentRound().addTrick(trick);
}

Seat GameEngine::determineTrickWinner(const Trick& trick) const
{
    const std::vector<PlayedCard>& playedCards = trick.getPlayedCards();

    if(playedCards.empty())
        throw std::logic_error("determineTrickWinner() needs at least one played card");

    std::size_t bestIndex = 0;

    for(std::size_t i = 1 ; i < playedCards.size() ; i++)
    {
        if(cardBeats(*playedCards[i].card, *playedCards[bestIndex].card, trick.getLeadSuit()))
            bestIndex = i;
    }

    // Read off the winning entry rather than counting seats round from the
    // leader: the trick records who played each card, so the position a card
    // was played in never has to be translated back into a seat.
    return playedCards[bestIndex].seat;
}

void GameEngine::setTrickLeaderSeat(Seat seat)
{
    requireStarted();

    scoreboard.getCurrentRound().setTrickLeaderSeat(seat);
}

void GameEngine::completeCurrentRound()
{
    requireStarted();

    if(scoreboard.getCurrentRoundIndex() + 1 >= scoreboard.getRoundCount())
    {
        status = GameStatus::Finished;
        phase = GamePhase::GameOver;
    }
    else
    {
        // The index has moved, so from here getCurrentRoundIndex() names the
        // NEXT round - which is why onRoundScored(), not onRoundComplete(), is
        // the hook a client draws a round's result from. The phase goes back to
        // Betting at that round's deal.
        scoreboard.incrementCurrentRound();
    }
}

void GameEngine::calculateScores()
{
    requireStarted();

    // Scoreboard::calculateScores accumulates into currentRoundScore and steps
    // the streak counters, neither with a guard - so a second call doubles the
    // round's points AND double-increments the streaks, walking a player on
    // their fifth consecutive hit straight from 4 to 6 so the == 5 bonus never
    // fires at all. That missing +-10 reads as a scoring-rule bug rather than
    // as a double call, and no assertion about round scores would see it. So
    // gate the whole function on the phase, not the arithmetic.
    //
    // Phase 2b of ENGINE_V4_PLAN.md tightens this to `phase != Playing` once
    // the duplicated test loop, which never sets a phase at all, is gone.
    if(phase == GamePhase::RoundScored)
        throw std::logic_error("GameEngine::calculateScores: this round has already been scored");

    scoreboard.calculateScores(players);

    if(phase == GamePhase::Playing)
        phase = GamePhase::RoundScored;
}

void GameEngine::commitRoundScores()
{
    scoreboard.commitRoundScores(players);
}

std::vector<std::pair<std::string, int>> GameEngine::getPlayerScores() const
{
    return scoreboard.getPlayerScores(players);
}

std::vector<std::pair<std::string, std::pair<int, int>>> GameEngine::getPlayerRoundScores() const
{
    std::vector<std::pair<std::string, std::pair<int, int>>> roundScores;
    
    for(const auto& player : players)
    {
        roundScores.emplace_back(player.getName(),
                               std::pair<int, int>(player.getCurrentRoundScore(),
                                             player.getTotalScore() + player.getCurrentRoundScore()));
    }
    
    return roundScores;
}

const PlayerList &GameEngine::getPlayers() const
{
    return players;
}


const Round &GameEngine::getCurrentRound() const
{
    requireStarted();

    return scoreboard.getCurrentRound();
}

std::optional<Seat> GameEngine::getActiveSeat() const
{
    return activeSeat;
}

unsigned int GameEngine::getCurrentTrickNumber() const
{
    requireStarted();

    return currentTrickNumber;
}

const Trick &GameEngine::getCurrentTrick() const
{
    requireStarted();

    return scoreboard.getCurrentRound().getCurrentTrick();
}

std::optional<Seat> GameEngine::getCurrentTrickLeader() const
{
    requireStarted();

    const Trick& trick = scoreboard.getCurrentRound().getCurrentTrick();

    if(trick.getPlayedCards().empty())
        return std::nullopt;

    // The very same call that will declare the winner once the trick is full -
    // it ranks whatever has been played so far and reads the seat off the
    // winning entry. Reimplementing the ranking here is how a "currently
    // winning" highlight ends up disagreeing with the trick's actual winner.
    return determineTrickWinner(trick);
}

std::optional<unsigned int> GameEngine::getBet(Seat seat) const
{
    requireStarted();

    return scoreboard.getCurrentRound().getBet(seat);
}

unsigned int GameEngine::getTricksWon(Seat seat) const
{
    requireStarted();

    return scoreboard.getCurrentRound().getTricksWon(seat);
}

void GameEngine::requireStarted() const
{
    if(!scoreboardInitialized)
        throw std::logic_error("GameEngine: the game has not been set up yet - "
                               "there is no current round to read");
}

const Deck &GameEngine::getDeck() const
{
    return deck;
}

unsigned int GameEngine::getCurrentRoundIndex() const
{
    requireStarted();

    return scoreboard.getCurrentRoundIndex();
}

unsigned int GameEngine::getRoundCount() const
{
    return scoreboard.getRoundCount();
}

RoundType GameEngine::getCurrentRoundType() const
{
    requireStarted();

    return scoreboard.getCurrentRound().getRoundType();
}

bool GameEngine::cardBeats(const Card& candidate, const Card& currentBest, Suit leadSuit) const
{
    // The ranking itself lives on CardValidator, so that a strategy weighing up
    // "would this card win?" reasons with the very rule that will later declare
    // the winner, and the two can never drift apart.
    return CardValidator::beats(candidate, currentBest, leadSuit, getCurrentTrumpCard());
}

// Dispatch. Each of these iterates `observers` directly rather than through a
// shared template, so that a stack trace names the callback that threw.
// Registering or removing an observer from inside one of these invalidates the
// iteration, which is why addObserver()/removeObserver() forbid it.

void GameEngine::notifyGameStarted()
{
    for(IGameObserver* observer : observers)
        observer->onGameStarted(*this);
}

void GameEngine::notifyRoundStarted()
{
    for(IGameObserver* observer : observers)
        observer->onRoundStarted(*this);
}

void GameEngine::notifyBetRequested(Seat seat)
{
    for(IGameObserver* observer : observers)
        observer->onBetRequested(*this, seat);
}

void GameEngine::notifyBetPlaced(Seat seat, unsigned int bet)
{
    for(IGameObserver* observer : observers)
        observer->onBetPlaced(*this, seat, bet);
}

void GameEngine::notifyBettingComplete()
{
    for(IGameObserver* observer : observers)
        observer->onBettingComplete(*this);
}

void GameEngine::notifyTrickStarted(unsigned int trickNumber, Seat leader)
{
    for(IGameObserver* observer : observers)
        observer->onTrickStarted(*this, trickNumber, leader);
}

void GameEngine::notifyCardRequested(Seat seat)
{
    for(IGameObserver* observer : observers)
        observer->onCardRequested(*this, seat);
}

void GameEngine::notifyCardPlayed(Seat seat, const Card& card)
{
    for(IGameObserver* observer : observers)
        observer->onCardPlayed(*this, seat, card);
}

void GameEngine::notifyTrickWon(Seat winner, unsigned int trickNumber)
{
    for(IGameObserver* observer : observers)
        observer->onTrickWon(*this, winner, trickNumber);
}

void GameEngine::notifyRoundScored()
{
    for(IGameObserver* observer : observers)
        observer->onRoundScored(*this);
}

void GameEngine::notifyRoundComplete()
{
    for(IGameObserver* observer : observers)
        observer->onRoundComplete(*this);
}

void GameEngine::notifyGameOver()
{
    for(IGameObserver* observer : observers)
        observer->onGameOver(*this);
}

void GameEngine::notifyGameStopped()
{
    for(IGameObserver* observer : observers)
        observer->onGameStopped(*this);
}

} // namespace romanian_whist
