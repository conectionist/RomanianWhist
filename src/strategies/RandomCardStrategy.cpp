#include <romanian_whist/strategies/RandomCardStrategy.h>

#include <romanian_whist/detail/RandomDraw.h>

namespace romanian_whist
{
RandomCardStrategy::RandomCardStrategy() : generator(std::random_device{}())
{}

RandomCardStrategy::RandomCardStrategy(std::uint32_t seed) : generator(seed)
{}

unsigned int RandomCardStrategy::getBestBet(const BetContext &context)
{
    if (context.hand.empty())
        return 0;

    // Anything from bidding nothing up to bidding every trick in the round.
    const unsigned int highest = static_cast<unsigned int>(context.hand.size());

    // Drawing from a range one short and then stepping over the barred value
    // picks uniformly among what is left, without rejecting and redrawing.
    const unsigned int choiceCount = context.forbiddenBet ? highest : highest + 1;

    unsigned int bet = detail::uniformIndex(generator, choiceCount);

    if (context.forbiddenBet && bet >= *context.forbiddenBet)
        bet++;

    return bet;
}

std::optional<Card> RandomCardStrategy::getBestChoice(const PlayContext &context)
{
    const std::vector<Card> legalCards = cardValidator.getLegalCards(context.hand,
                                                                context.trump,
                                                                context.leadSuit);

    if (legalCards.empty())
        return std::nullopt;

    return legalCards[detail::uniformIndex(generator, static_cast<unsigned int>(legalCards.size()))];
}

} // namespace romanian_whist
