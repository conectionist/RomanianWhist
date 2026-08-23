#ifndef CARD_VALIDATOR_H
#define CARD_VALIDATOR_H

#include "Card.h"

#include <vector>

using std::vector;

class CardValidator
{
public:
    vector<Card*> getLegalCards(const vector<Card*>& hand, Card* trump, const Suit* leadSuit) const;

private:
    bool hasSuit(const vector<Card*>& hand, Suit suit) const;
};

#endif
