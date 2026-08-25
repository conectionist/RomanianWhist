#include <romanian_whist/AiMoveProvider.h>

namespace romanian_whist
{
AiMoveProvider::AiMoveProvider(std::unique_ptr<IStrategy> _strategy) : strategy(std::move(_strategy))
{}

unsigned int AiMoveProvider::makeBet(const std::vector<Card *> &hand, Card *trump, bool isFirstPlayer)
{
    return strategy->getBestBet(hand, trump, isFirstPlayer);
}

Card *AiMoveProvider::playCard(const std::vector<Card *> &hand, Card *trump, const Suit *leadSuit)
{
    return strategy->getBestChoice(hand, trump, leadSuit);
}

} // namespace romanian_whist
