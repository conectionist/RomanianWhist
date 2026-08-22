#ifndef ROUND_H
#define ROUND_H

#include "Hand.h"
#include "Util.h"

#include <vector>
#include <unordered_map>

using std::vector;
using std::unordered_map;

class Player;

struct Bet
{
    unsigned int guess;
    unsigned int actual;
};

class Round
{
private:
    vector<Hand> hands;
    unordered_map<string, Bet> bets;
    Card* trump;
    unsigned int handCount;
    RoundType type;
    Player* firstPlayer;

public:
    Round(unsigned int _handCount, Player* player, RoundType _type = RoundType::Normal);
    void addHand(const Hand& hand);
    void setBet(Player* player, unsigned int guess);
    void setResult(Player* player, unsigned int wonHands);
    void setTrumpCard(Card* card);
    Card* getTrumpCard();
    unsigned int getHandCount();
    void setFirstPlayer(Player* player);
    Player* getFirstPlayer() const;
    void setRoundType(RoundType _type);
};

#endif
