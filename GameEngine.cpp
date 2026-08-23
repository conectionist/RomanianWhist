#include "GameEngine.h"

#include <utility>

GameEngine::GameEngine() : status(GameStatus::NotStarted)
{}

void GameEngine::addPlayer(Player &&player)
{
    players.addPlayer(std::move(player));
}

void GameEngine::addPlayer(const string &name, unique_ptr<IMoveProvider> moveProvider)
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

GameStatus GameEngine::getStatus()
{
    return status;
}

void GameEngine::setStatus(GameStatus _status)
{
    status = _status;
}

void GameEngine::start()
{
}

bool GameEngine::isInProgress()
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

    unsigned int gameCount = scoreboard.getCurrentRound().getHandCount();
    unsigned int index = 0;

    for(unsigned int i = 0 ; i < gameCount ; i++)
    {
        for(unsigned int j = 0 ; j < players.size() ; j++)
        {
            index = gameCount * j + i;
            players[j].addCardToHand(deck.getCardAt(index));
        }
    }

    if(gameCount < 8)
    {
        scoreboard.getCurrentRound().setTrumpCard(deck.getCardAt(index + 1));
    }
}

Card *GameEngine::getCurrentTrumpCard()
{
    return scoreboard.getCurrentRound().getTrumpCard();
}

// void GameEngine::createPlayers(const vector<string> &playerNames)
// {
//     players.createPlayers(playerNames);
// }

PlayerList::iterator GameEngine::getFirstPlayerOfTheRound()
{
    return scoreboard.getCurrentRound().getFirstPlayer();
}

PlayerList::iterator GameEngine::getNextPlayer(PlayerList::iterator player)
{
    return players.next(player);
}

unsigned int GameEngine::getPlayerCount()
{
    return players.size();
}

void GameEngine::placeBet(PlayerList::iterator player, unsigned int bet)
{
    scoreboard.getCurrentRound().setBet(player, bet);
}

void GameEngine::setResult(PlayerList::iterator player, unsigned int wonHands)
{
    scoreboard.getCurrentRound().setResult(player, wonHands);
}

void GameEngine::clearAllPlayerHands()
{
    for(auto& player : players)
        player.clearHand();
}

unsigned int GameEngine::getCurrentRoundHandCount()
{
    return scoreboard.getCurrentRound().getHandCount();
}

void GameEngine::addHandToCurrentRound(const Hand& hand)
{
    scoreboard.getCurrentRound().addHand(hand);
}

PlayerList::iterator GameEngine::determineTrickWinner(const Hand& trick, PlayerList::iterator firstPlayer)
{
    const auto playedCards = trick.getPlayedCards();
    unsigned int bestIndex = 0;

    for(unsigned int i = 1 ; i < playedCards.size() ; i++)
    {
        if(cardBeats(*playedCards[i], *playedCards[bestIndex], trick.getDownSuit()))
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

vector<pair<string, int>> GameEngine::getPlayerScores() const
{
    return scoreboard.getPlayerScores(players);
}

vector<pair<string, pair<int, int>>> GameEngine::getPlayerRoundScores() const
{
    vector<pair<string, pair<int, int>>> roundScores;
    
    for(const auto& player : players)
    {
        roundScores.emplace_back(player.getName(), 
                               pair<int, int>(player.getCurrentRoundScore(), 
                                             player.getTotalScore()));
    }
    
    return roundScores;
}

bool GameEngine::cardBeats(const Card& candidate, const Card& currentBest, Suit ledSuit)
{
    const Card* trump = getCurrentTrumpCard();
    const bool candidateIsTrump = trump && candidate.suit == trump->suit;
    const bool bestIsTrump = trump && currentBest.suit == trump->suit;

    if(candidateIsTrump || bestIsTrump)
    {
        if(candidateIsTrump != bestIsTrump)
            return candidateIsTrump;

        return static_cast<int>(candidate.rank) > static_cast<int>(currentBest.rank);
    }

    const bool candidateIsLed = candidate.suit == ledSuit;
    const bool bestIsLed = currentBest.suit == ledSuit;

    if(candidateIsLed != bestIsLed)
        return candidateIsLed;

    if(candidateIsLed)
        return static_cast<int>(candidate.rank) > static_cast<int>(currentBest.rank);

    return false;
}
