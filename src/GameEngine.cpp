#include <romanian_whist/GameEngine.h>

#include <romanian_whist/CardValidator.h>

#include <utility>

namespace romanian_whist
{
GameEngine::GameEngine() : status(GameStatus::NotStarted)
{}

void GameEngine::addPlayer(const std::string &name, std::unique_ptr<IMoveProvider> moveProvider)
{
    players.addPlayer(name, std::move(moveProvider));
}

void GameEngine::initializeScoreboard(const GameStructure &structure, 
                                      bool endWithForeheadAndHidden, 
                                      bool all1GamesAreForehead)
{
    scoreboard.initialize(structure, endWithForeheadAndHidden, all1GamesAreForehead, players);
}

void GameEngine::initializeDeck(unsigned int playerCount)
{
    for(int s = 0 ; s < 4 ; s++)
        for(int r = 1 + (6 - playerCount) * 2 ; r < 13 ; r++)
        {
            Card card(static_cast<Rank>(r), static_cast<Suit>(s));
            deck.addCard(std::move(card));
        }   
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
    deck.shuffle();
}

void GameEngine::dealCards()
{
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

PlayerList::iterator GameEngine::getFirstPlayerOfTheRound()
{
    return scoreboard.getCurrentRound().getFirstPlayer();
}

PlayerList::iterator GameEngine::getNextPlayer(PlayerList::iterator player)
{
    return players.next(player);
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

    // Counted by walking the players rather than by asking the bets map for its
    // size, because setResult() writes into that same map and would inflate it
    // once the round is under way.
    for(const auto& player : players)
    {
        if(round.hasBet(player.getName()))
        {
            betsPlaced++;
            total += round.getBet(player.getName());
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

void GameEngine::placeBet(PlayerList::iterator player, unsigned int bet)
{
    scoreboard.getCurrentRound().setBet(player, bet);
}

void GameEngine::setResult(PlayerList::iterator player, unsigned int wonTricks)
{
    scoreboard.getCurrentRound().setResult(player, wonTricks);
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

PlayerList::iterator GameEngine::determineTrickWinner(const Trick& trick, PlayerList::iterator firstPlayer)
{
    const auto playedCards = trick.getPlayedCards();
    unsigned int bestIndex = 0;

    for(unsigned int i = 1 ; i < playedCards.size() ; i++)
    {
        if(cardBeats(*playedCards[i], *playedCards[bestIndex], trick.getLeadSuit()))
            bestIndex = i;
    }

    return players.advanceCircular(firstPlayer, bestIndex);
}

void GameEngine::setFirstPlayerOfTheRound(PlayerList::iterator player)
{
    scoreboard.getCurrentRound().setFirstPlayer(player);
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

const Round &GameEngine::getCurrentRound() const
{
    return scoreboard.getCurrentRound();
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

bool GameEngine::cardBeats(const Card& candidate, const Card& currentBest, Suit leadSuit)
{
    // The ranking itself lives on CardValidator, so that a strategy weighing up
    // "would this card win?" reasons with the very rule that will later declare
    // the winner, and the two can never drift apart.
    return CardValidator::beats(candidate, currentBest, leadSuit, getCurrentTrumpCard());
}

} // namespace romanian_whist
