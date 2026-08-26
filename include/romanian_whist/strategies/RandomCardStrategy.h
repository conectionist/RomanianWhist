#ifndef RANDOM_CARD_STRATEGY_H
#define RANDOM_CARD_STRATEGY_H

#include <romanian_whist/strategies/IStrategy.h>

namespace romanian_whist
{
class RandomCardStrategy : public IStrategy
{
public:
    unsigned int getBestBet(const BetContext& context) override;
    Card* getBestChoice(const PlayContext& context) override;
};

} // namespace romanian_whist

#endif
