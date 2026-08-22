#ifndef DECK_H
#define DECK_H

#include "Card.h"

#include <vector>

using std::vector;

class Deck
{
private:
    vector<Card> deck;

public:
    void addCard(Card&& card);
    void shuffle();
    Card* getCardAt(unsigned int i);
};

#endif
