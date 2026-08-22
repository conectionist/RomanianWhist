#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include "Round.h"
#include "PlayerList.h"

#include <vector>

using std::vector;

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

private:
    void addRound(Round&& round);
};

#endif
