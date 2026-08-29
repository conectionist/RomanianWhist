#include <romanian_whist/GameEngine.h>

#include <romanian_whist/CardValidator.h>

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
    status = _status;
}

bool GameEngine::isInProgress() const
{
    return status == GameStatus::InProgress;
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
    return scoreboard.getCurrentRound().getTrumpCard();
}

const Card *GameEngine::getCurrentTrumpCard() const
{
    return scoreboard.getCurrentRound().getTrumpCard();
}

Seat GameEngine::getRoundLeaderSeat() const
{
    return scoreboard.getCurrentRound().getLeaderSeat();
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
    if(bet > getCurrentRoundTrickCount())
        return false;

    const std::optional<unsigned int> forbidden = getForbiddenBet();

    return !forbidden || bet != *forbidden;
}

void GameEngine::placeBet(Seat seat, unsigned int bet)
{
    scoreboard.getCurrentRound().setBet(seat, bet);
}

void GameEngine::clearAllPlayerHands()
{
    for(auto& player : players)
        player.clearHand();
}

unsigned int GameEngine::getCurrentRoundTrickCount() const
{
    return scoreboard.getCurrentRound().getTrickCount();
}

void GameEngine::addTrickToCurrentRound(const Trick& trick)
{
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

void GameEngine::setRoundLeaderSeat(Seat seat)
{
    scoreboard.getCurrentRound().setLeaderSeat(seat);
}

void GameEngine::completeCurrentRound()
{
    if(scoreboard.getCurrentRoundIndex() + 1 >= scoreboard.getRoundCount())
        status = GameStatus::Finished;
    else
        scoreboard.incrementCurrentRound();
}

void GameEngine::calculateScores()
{
    scoreboard.calculateScores(players);
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

Player &GameEngine::getPlayer(Seat seat)
{
    return players.at(seat.index);
}

const Round &GameEngine::getCurrentRound() const
{
    return scoreboard.getCurrentRound();
}

const Deck &GameEngine::getDeck() const
{
    return deck;
}

unsigned int GameEngine::getCurrentRoundIndex() const
{
    return scoreboard.getCurrentRoundIndex();
}

unsigned int GameEngine::getRoundCount() const
{
    return scoreboard.getRoundCount();
}

RoundType GameEngine::getCurrentRoundType() const
{
    return scoreboard.getCurrentRound().getRoundType();
}

bool GameEngine::cardBeats(const Card& candidate, const Card& currentBest, Suit leadSuit) const
{
    // The ranking itself lives on CardValidator, so that a strategy weighing up
    // "would this card win?" reasons with the very rule that will later declare
    // the winner, and the two can never drift apart.
    return CardValidator::beats(candidate, currentBest, leadSuit, getCurrentTrumpCard());
}

} // namespace romanian_whist
