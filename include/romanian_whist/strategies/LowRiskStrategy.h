#ifndef LOW_RISK_STRATEGY_H
#define LOW_RISK_STRATEGY_H

#include <romanian_whist/strategies/IStrategy.h>

namespace romanian_whist
{
// Plays it safe: it counts the tricks its hand will take whether it likes it or
// not, bids exactly those - usually none - and then plays to that bid, going
// after tricks only while it still owes some and ducking every trick after
// that. See DuckingStrategy for the variant that never chases a trick at all.
class LowRiskStrategy : public IStrategy
{
public:
    unsigned int getBestBet(const BetContext& context) override;
    std::optional<Card> getBestChoice(const PlayContext& context) override;
};

} // namespace romanian_whist

#endif
