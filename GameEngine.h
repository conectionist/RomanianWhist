#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "PlayerList.h"
#include "Scoreboard.h"
#include "Deck.h"

#include <string>
#include <vector>
#include <utility>

using std::string;
using std::vector;
using std::pair;

enum class GameStatus
{
    NotStarted,
    InProgress,
    Finished
};

class GameEngine
{
private:
    PlayerList players;
    Scoreboard scoreboard;
    Deck deck;
    GameStatus status;

public:
    GameEngine();
    void addPlayer(Player&& player);
    void addPlayer(const string& name, unique_ptr<IMoveProvider> moveProvider);
    void initializeScoreboard(const GameStructure& structure, 
                              bool endWithForeheadAndHidden, 
                              bool all1GamesAreForehead);
    void initializeDeck(unsigned int playerCount);
    GameStatus getStatus();
    void setStatus(GameStatus _status);
    void start();
    bool isInProgress();
    void shuffleDeck();
    void dealCards();
    Card* getCurrentTrumpCard();
    //void createPlayers(const vector<string>& playerNames);
    PlayerList::iterator getFirstPlayerOfTheRound();
    PlayerList::iterator getNextPlayer(PlayerList::iterator player);
    unsigned int getPlayerCount();
    void placeBet(PlayerList::iterator player, unsigned int bet);
    void setResult(PlayerList::iterator player, unsigned int wonHands);
    unsigned int getCurrentRoundHandCount();
    void addHandToCurrentRound(const Hand& hand);
    PlayerList::iterator determineTrickWinner(const Hand& trick, PlayerList::iterator firstPlayer);
    void setFirstPlayerOfTheRound(PlayerList::iterator player);
    void completeCurrentRound();
    void calculateScores();
    void commitRoundScores();
    
    // Data access methods for display purposes
    vector<std::pair<string, int>> getPlayerScores() const;
    vector<std::pair<string, std::pair<int, int>>> getPlayerRoundScores() const;

private:
    void clearAllPlayerHands();
    bool cardBeats(const Card& candidate, const Card& currentBest, Suit ledSuit);
};

#endif
