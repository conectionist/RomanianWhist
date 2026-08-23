#ifndef FIRST_CARD_STRATEGY_H
#define FIRST_CARD_STRATEGY_H

#include "IStrategy.h"

class FirstCardStrategy : public IStrategy
{
public:
    unsigned int getBestBet(const vector<Card*>& hand, Card* trump, bool isFirstPlayer) override;
    Card* getBestChoice(vector<Card*>& hand, Card* trump, const Suit* downSuit) override;
};

#endif
