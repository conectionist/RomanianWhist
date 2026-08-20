#include "Deck.h"

Deck::Deck()
{
    for(int s = 0 ; s < 4 ; s++)
        for(int r = 0 ; r < 13 ; r++)
            deck[s * 13 + r] = Card(static_cast<Rank>(r), static_cast<Suit>(s));
}

void Deck::shuffle()
{
}
