#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "Player.h"
#include "Scoreboard.h"
#include "Deck.h"

#include <vector>

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
// TODO: replace this with a custor PlayerList class that supports iterators and replace every Player* thoughout the codebase with PlayerList::iterator
    vector<Player> players;
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
    Player* getFirstPlayerOfTheRound();
    unsigned int getPlayerCount();
    void placeBet(Player* player, unsigned int bet);

private:
    void clearAllPlayerHands();
};

#endif
