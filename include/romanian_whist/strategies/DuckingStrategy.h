#ifndef DUCKING_STRATEGY_H
#define DUCKING_STRATEGY_H

#include <romanian_whist/strategies/IStrategy.h>

namespace romanian_whist
{
// Bids nothing and spends the round trying to take nothing, dumping its high
// cards at the moments they cannot win. It never chases a trick, not even one
// it has bid for, which makes it the simpler and the more single-minded of the
// two low-risk strategies. See LowRiskStrategy for the one that plays to its
// bid.
class DuckingStrategy : public IStrategy
{
public:
    unsigned int getBestBet(const BetContext& context) override;
    std::optional<Card> getBestChoice(const PlayContext& context) override;
};

} // namespace romanian_whist

#endif
