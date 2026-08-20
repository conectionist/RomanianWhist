#ifndef DECK_H
#define DECK_H

#include "Card.h"

#include <array>

using std::array;

class Deck
{
private:
    array<Card, 52> deck;

public:
    Deck();
    void shuffle();
};

#endif
