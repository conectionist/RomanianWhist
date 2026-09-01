#ifndef FIRST_CARD_STRATEGY_H
#define FIRST_CARD_STRATEGY_H

#include <romanian_whist/strategies/IStrategy.h>

namespace romanian_whist
{
class FirstCardStrategy : public IStrategy
{
public:
    unsigned int getBestBet(const BetContext& context) override;
    std::optional<Card> getBestChoice(const PlayContext& context) override;
};

} // namespace romanian_whist

#endif
