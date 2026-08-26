#include <romanian_whist/strategies/DuckingStrategy.h>

#include <romanian_whist/strategies/TrickHeuristics.h>

namespace romanian_whist
{
unsigned int DuckingStrategy::getBestBet(const BetContext &context)
{
    // Always nothing. When the final-bidder rule bars zero it bids one and
    // takes the resulting minus one on the chin, rather than spend the round
    // hunting a trick it has no intention of winning. A round always has at
    // least one trick, so one is always available when zero is barred.
    return context.forbiddenBet == 0u ? 1u : 0u;
}

Card *DuckingStrategy::getBestChoice(const PlayContext &context)
{
    const std::vector<Card*> legalCards = cardValidator.getLegalCards(context.hand,
                                                                      context.trump,
                                                                      context.leadSuit);

    return heuristics::chooseDuckingCard(context, legalCards);
}

} // namespace romanian_whist
