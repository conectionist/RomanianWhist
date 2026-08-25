#include <romanian_whist/strategies/FirstCardStrategy.h>
#include <algorithm>

namespace romanian_whist
{
unsigned int FirstCardStrategy::getBestBet(const std::vector<Card *> &hand, Card *trump, bool isFirstPlayer)
{
    return 0;
}

Card *FirstCardStrategy::getBestChoice(std::vector<Card *> &hand, Card *trump, const Suit *leadSuit)
{
    std::vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, leadSuit);

    return legalCards[0];
}

} // namespace romanian_whist
