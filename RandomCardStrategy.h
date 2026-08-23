#ifndef RANDOM_CARD_STRATEGY_H
#define RANDOM_CARD_STRATEGY_H

#include "IStrategy.h"

class RandomCardStrategy : public IStrategy
{
public:
    unsigned int getBestBet(const vector<Card*>& hand, Card* trump, bool isFirstPlayer) override;
    Card* getBestChoice(vector<Card*>& hand, Card* trump, const Suit* downSuit) override;
};

#endif
