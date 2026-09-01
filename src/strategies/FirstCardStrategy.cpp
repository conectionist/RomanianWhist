#include <romanian_whist/strategies/FirstCardStrategy.h>
#include <algorithm>

namespace romanian_whist
{
unsigned int FirstCardStrategy::getBestBet(const BetContext &context)
{
    // Bets nothing, in keeping with a strategy that puts no thought into its
    // play either. A round always has at least one trick, so when zero is the
    // bid barred from the final bidder, one is always still available.
    return context.forbiddenBet == 0u ? 1u : 0u;
}

std::optional<Card> FirstCardStrategy::getBestChoice(const PlayContext &context)
{
    const std::vector<Card> legalCards = cardValidator.getLegalCards(context.hand,
                                                                context.trump,
                                                                context.leadSuit);

    if(legalCards.empty())
        return std::nullopt;

    return legalCards[0];
}

} // namespace romanian_whist
