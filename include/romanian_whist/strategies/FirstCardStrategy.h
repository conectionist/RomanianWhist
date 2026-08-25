#ifndef FIRST_CARD_STRATEGY_H
#define FIRST_CARD_STRATEGY_H

#include <romanian_whist/strategies/IStrategy.h>

namespace romanian_whist
{
class FirstCardStrategy : public IStrategy
{
public:
    unsigned int getBestBet(const std::vector<Card*>& hand, Card* trump, bool isFirstPlayer) override;
    Card* getBestChoice(const std::vector<Card*>& hand, Card* trump, const Suit* leadSuit) override;
};

} // namespace romanian_whist

#endif
