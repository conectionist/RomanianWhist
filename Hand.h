#ifndef HAND_H
#define HAND_H

#include "Card.h"

#include <vector>

using std::vector;

class Hand
{
private:
    vector<Card*> playedCards;

public:
    void addPlayedCard(Card* card);
    vector<Card*> getPlayedCards();
};

#endif
