#include "AiMoveProvider.h"

#include <algorithm>

AiMoveProvider::AiMoveProvider(unique_ptr<IStrategy> _strategy) : strategy(std::move(_strategy))
{}

unsigned int AiMoveProvider::makeBet(const vector<Card *> &hand, Card *trump, bool isFirstPlayer)
{
    return strategy->getBestBet(hand, trump, isFirstPlayer);
}

Card *AiMoveProvider::playCard(vector<Card *> &hand, Card *trump, const Suit *downSuit)
{
    auto* card = strategy->getBestChoice(hand, trump, downSuit);
    hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
    return card;
}
