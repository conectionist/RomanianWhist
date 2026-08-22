#ifndef HAND_H
#define HAND_H

#include "Card.h"
#include "PlayerList.h"

#include <vector>

using std::vector;

class Hand
{
private:
    vector<Card*> playedCards;
    Suit downSuit;
    PlayerList::iterator winner;

public:
    void addPlayedCard(Card* card);
    vector<Card*> getPlayedCards() const;

    void setDownSuit(Suit suit);
    const Suit& getDownSuit() const;

    void setWinner(PlayerList::iterator player);
    PlayerList::iterator getWinner();
};

#endif
