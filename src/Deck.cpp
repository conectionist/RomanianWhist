#include <romanian_whist/Deck.h>

#include <algorithm>
#include <random>

namespace romanian_whist
{
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

Card& Deck::operator[](size_type index)
{
    return deck[index];
}

const Card& Deck::operator[](size_type index) const
{
    return deck[index];
}

} // namespace romanian_whist
