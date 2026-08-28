#ifndef DECK_H
#define DECK_H

#include <romanian_whist/Card.h>

#include <cstddef>
#include <random>
#include <vector>

namespace romanian_whist
{
class Deck
{
private:
    std::vector<Card> deck;

public:
    using size_type = std::vector<Card>::size_type;
    using const_iterator = std::vector<Card>::const_iterator;

    void addCard(Card&& card);
    void shuffle(std::mt19937& generator);

    Card& operator[](size_type index);
    const Card& operator[](size_type index) const;

    size_type size() const;
    const_iterator begin() const;
    const_iterator end() const;
};

} // namespace romanian_whist

#endif
