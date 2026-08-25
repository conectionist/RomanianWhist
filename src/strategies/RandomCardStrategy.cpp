#include <romanian_whist/strategies/RandomCardStrategy.h>

#include <random>

namespace romanian_whist
{
unsigned int RandomCardStrategy::getBestBet(const BetContext &context)
{
    if (context.hand.empty())
        return 0;

    // Anything from bidding nothing up to bidding every trick in the round.
    const unsigned int highest = static_cast<unsigned int>(context.hand.size());

    // Drawing from a range one short and then stepping over the barred value
    // picks uniformly among what is left, without rejecting and redrawing.
    const unsigned int choiceCount = context.forbiddenBet ? highest : highest + 1;

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<unsigned int> dist(0, choiceCount - 1);

    unsigned int bet = dist(generator);

    if (context.forbiddenBet && bet >= *context.forbiddenBet)
        bet++;

    return bet;
}

Card *RandomCardStrategy::getBestChoice(const std::vector<Card *> &hand, Card *trump, const Suit *leadSuit)
{
    std::vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, leadSuit);

    if (legalCards.empty())
        return nullptr;

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<std::size_t> dist(0, legalCards.size() - 1);

    return legalCards[dist(generator)];
}

} // namespace romanian_whist
