#ifndef ROUND_H
#define ROUND_H

#include "Hand.h"
#include "PlayerList.h"
#include "Util.h"

#include <vector>
#include <unordered_map>

using std::vector;
using std::unordered_map;

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
    PlayerList::iterator firstPlayer;

public:
    Round(unsigned int _handCount, PlayerList::iterator player, RoundType _type = RoundType::Normal);
    void addHand(const Hand& hand);
    void setBet(PlayerList::iterator player, unsigned int guess);
    void setResult(PlayerList::iterator player, unsigned int wonHands);
    void setTrumpCard(Card* card);
    Card* getTrumpCard();
    unsigned int getHandCount();
    void setFirstPlayer(PlayerList::iterator player);
    PlayerList::iterator getFirstPlayer() const;
    void setRoundType(RoundType _type);
};

#endif
