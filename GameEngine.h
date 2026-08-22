#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "PlayerList.h"
#include "Scoreboard.h"
#include "Deck.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

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
    void createPlayers(const vector<string>& playerNames);
    PlayerList::iterator getFirstPlayerOfTheRound();
    PlayerList::iterator getNextPlayer(PlayerList::iterator player);
    unsigned int getPlayerCount();
    void placeBet(PlayerList::iterator player, unsigned int bet);

private:
    void clearAllPlayerHands();
};

#endif
