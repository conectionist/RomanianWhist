#include "FirstCardStrategy.h"
#include <algorithm>

unsigned int FirstCardStrategy::getBestBet(const vector<Card *> &hand, Card *trump, bool isFirstPlayer)
{
    return 0;
}

Card *FirstCardStrategy::getBestChoice(vector<Card *> &hand, Card *trump, const Suit *leadSuit)
{
    vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, leadSuit);

    return legalCards[0];
}
