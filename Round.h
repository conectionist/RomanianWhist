#ifndef ROUND_H
#define ROUND_H

#include "Hand.h"

#include <vector>
#include <utility>

using std::vector;
using std::pair;

class Player;

class Round
{
private:
    vector<Hand> hands;
    vector<pair<Player*, int>> results;

public:
    void addHand(const Hand& hand);
    void addResult(Player* player, int wonHands);
};

#endif
