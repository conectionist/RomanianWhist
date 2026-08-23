#ifndef DECK_H
#define DECK_H

#include "Card.h"

#include <cstddef>
#include <vector>

using std::vector;

class Deck
{
private:
    vector<Card> deck;

public:
    using size_type = vector<Card>::size_type;

    void addCard(Card&& card);
    void shuffle();

    Card& operator[](size_type index);
    const Card& operator[](size_type index) const;
};

#endif
