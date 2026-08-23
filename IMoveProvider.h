#ifndef IMOVEPROVIDER_H
#define IMOVEPROVIDER_H

#include "Card.h"

#include <vector>

using std::vector;

class IMoveProvider
{
public:
    virtual ~IMoveProvider() = default;
    virtual unsigned int makeBet(const vector<Card*>& hand, Card* trump, bool isFirstPlayer) = 0;
    virtual Card* playCard(vector<Card*>& hand, Card* trump, const Suit* leadSuit) = 0;
};

#endif
