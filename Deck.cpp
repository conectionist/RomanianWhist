#include "Deck.h"

void Deck::addCard(Card&& card)
{
    deck.push_back(std::move(card));
}

void Deck::shuffle()
{
}

Card *Deck::getCardAt(unsigned int i)
{
    return &deck[i];
}
