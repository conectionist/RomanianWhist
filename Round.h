#ifndef ROUND_H
#define ROUND_H

#include "Hand.h"
#include "Util.h"

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
    Card* trump;
    unsigned int handCount;
    RoundType type;
    const Player* firstPlayer;

public:
    Round(unsigned int _handCount, const Player* player, RoundType _type = RoundType::Normal);
    void addHand(const Hand& hand);
    void addResult(Player* player, int wonHands);
    void setTrumpCard(Card* card);
    Card* getTrumpCard();
    unsigned int getHandCount();
    void setFirstPlayer(const Player* player);
    const Player* getFirstPlayer() const;
    void setRoundType(RoundType _type);
};

#endif
