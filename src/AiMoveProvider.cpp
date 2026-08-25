#include <romanian_whist/AiMoveProvider.h>

#include <algorithm>

namespace romanian_whist
{
AiMoveProvider::AiMoveProvider(std::unique_ptr<IStrategy> _strategy) : strategy(std::move(_strategy))
{}

unsigned int AiMoveProvider::makeBet(const std::vector<Card *> &hand, Card *trump, bool isFirstPlayer)
{
    return strategy->getBestBet(hand, trump, isFirstPlayer);
}

Card *AiMoveProvider::playCard(std::vector<Card *> &hand, Card *trump, const Suit *leadSuit)
{
    auto* card = strategy->getBestChoice(hand, trump, leadSuit);
    hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
    return card;
}

} // namespace romanian_whist
