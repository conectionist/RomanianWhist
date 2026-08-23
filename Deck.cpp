#include "Deck.h"

#include <algorithm>
#include <random>

void Deck::addCard(Card&& card)
{
    deck.push_back(std::move(card));
}

void Deck::shuffle()
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::shuffle(deck.begin(), deck.end(), generator);
}

Card *Deck::getCardAt(unsigned int i)
{
    return &deck[i];
}

Card& Deck::operator[](size_type index)
{
    return deck[index];
}

const Card& Deck::operator[](size_type index) const
{
    return deck[index];
}
