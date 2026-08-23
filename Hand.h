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
    Suit leadSuit;
    PlayerList::iterator winner;

public:
    void addPlayedCard(Card* card);
    vector<Card*> getPlayedCards() const;

    void setLeadSuit(Suit suit);
    const Suit& getLeadSuit() const;

    void setWinner(PlayerList::iterator player);
    PlayerList::iterator getWinner();
};

#endif
