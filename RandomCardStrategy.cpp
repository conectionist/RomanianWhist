#include "RandomCardStrategy.h"

#include <random>

unsigned int RandomCardStrategy::getBestBet(const vector<Card *> &hand, Card *trump, bool isFirstPlayer)
{
    return 0;
}

Card *RandomCardStrategy::getBestChoice(vector<Card *> &hand, Card *trump, const Suit *leadSuit)
{
    vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, leadSuit);

    if (legalCards.empty())
        return nullptr;

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<std::size_t> dist(0, legalCards.size() - 1);

    return legalCards[dist(generator)];
}

