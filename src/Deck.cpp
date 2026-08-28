#include <romanian_whist/Deck.h>

#include <romanian_whist/detail/RandomDraw.h>

#include <utility>

namespace romanian_whist
{
void Deck::addCard(Card&& card)
{
    deck.push_back(std::move(card));
}

void Deck::shuffle(std::mt19937& generator)
{
    for(size_type i = deck.size() - 1 ; i > 0 ; i--)
    {
        const size_type j = detail::uniformIndex(generator, static_cast<unsigned int>(i + 1));
        std::swap(deck[i], deck[j]);
    }
}

Card& Deck::operator[](size_type index)
{
    return deck[index];
}

const Card& Deck::operator[](size_type index) const
{
    return deck[index];
}

Deck::size_type Deck::size() const
{
    return deck.size();
}

Deck::const_iterator Deck::begin() const
{
    return deck.begin();
}

Deck::const_iterator Deck::end() const
{
    return deck.end();
}

} // namespace romanian_whist
