#include "Scoreboard.h"

#include <utility>

Scoreboard::Scoreboard() : currentRound(0)
{
}

void Scoreboard::addRound(const Round &round)
{
    rounds.push_back(round);
}

void Scoreboard::addRound(Round &&round)
{
    rounds.push_back(std::move(round));
}

Round &Scoreboard::getRound(int i)
{
    return rounds[i];
}

void Scoreboard::incrementCurrentRound()
{
    currentRound++;
}

Round &Scoreboard::getCurrentRound()
{
    return getRound(currentRound);
}
