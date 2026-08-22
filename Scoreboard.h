#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include "Round.h"
#include "PlayerList.h"

#include <vector>
#include <utility>

using std::vector;
using std::pair;

enum class GameStructure
{
    S_181,
    S_818
};

class Scoreboard
{
private:
    vector<Round> rounds;
    unsigned int currentRound;

public:
    Scoreboard();
    void initialize(const GameStructure& structure, 
                    bool endWithForeheadAndHidden, 
                    bool all1GamesAreForehead,
                    PlayerList& players);
    
    Round& getRound(int i);
    void incrementCurrentRound();
    Round& getCurrentRound();
    vector<Round> getAllRounds();
    unsigned int getRoundCount() const;
    unsigned int getCurrentRoundIndex() const;
    
    void calculateScores(PlayerList& players);
    void commitRoundScores(PlayerList& players);
    
    // Data access methods for display purposes
    vector<std::pair<string, int>> getPlayerScores(const PlayerList& players) const;

private:
    void addRound(Round&& round);
    int calculateRoundScore(unsigned int bid, unsigned int actual) const;
    bool shouldCountForStreaks(const Round& round) const;
};

#endif
