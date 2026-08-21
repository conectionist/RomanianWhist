#ifndef HAND_H
#define HAND_H

#include "Card.h"

#include <vector>

using std::vector;

class Player;

class Hand
{
private:
    vector<Card*> playedCards;
    Suit downSuit;
    Player* winner;

public:
    void addPlayedCard(Card* card);
    vector<Card*> getPlayedCards();

    void setDownSuit(Suit suit);
    Suit getDownSuit();

    void setWinner(Player* player);
    Player* getWinner();
};

#endif
