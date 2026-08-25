#include <romanian_whist/strategies/RandomCardStrategy.h>

#include <random>

namespace romanian_whist
{
unsigned int RandomCardStrategy::getBestBet(const std::vector<Card *> &hand, Card *trump, bool isFirstPlayer)
{
    return 0;
}

Card *RandomCardStrategy::getBestChoice(std::vector<Card *> &hand, Card *trump, const Suit *leadSuit)
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
