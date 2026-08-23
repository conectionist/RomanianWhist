#include "FirstCardStrategy.h"
#include <algorithm>

unsigned int FirstCardStrategy::getBestBet(const vector<Card *> &hand, Card *trump, bool isFirstPlayer)
{
    return 0;
}

Card *FirstCardStrategy::getBestChoice(vector<Card *> &hand, Card *trump, const Suit *downSuit)
{
    vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, downSuit);

    return legalCards[0];
}
