#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include "Round.h"

#include <vector>

using std::vector;

class Scoreboard
{
private:
    vector<Round> rounds;
    unsigned int currentRound;

public:
    Scoreboard();
    void addRound(const Round& round);
    void addRound(Round&& round);
    Round& getRound(int i);
    void incrementCurrentRound();
    Round& getCurrentRound();
};

#endif
