#ifndef DECK_H
#define DECK_H

#include <romanian_whist/Card.h>

#include <cstddef>
#include <vector>

namespace romanian_whist
{
class Deck
{
private:
    std::vector<Card> deck;

public:
    using size_type = std::vector<Card>::size_type;

    void addCard(Card&& card);
    void shuffle();

    Card& operator[](size_type index);
    const Card& operator[](size_type index) const;
};

} // namespace romanian_whist

#endif
