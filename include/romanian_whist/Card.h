#ifndef CARD_H
#define CARD_H

#include <cstdint>
#include <string>

namespace romanian_whist
{
enum class Rank : std::int8_t
{
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven,
    Eight,
    Nine,
    Ten,
    Jack,
    Queen,
    King,
    Ace
};

enum class Suit : std::int8_t
{
    Hearts,
    Diamonds,
    Spades,
    Clubs
};

struct Card
{
    Rank rank;
    Suit suit;

    Card();
    Card(Rank _rank, Suit _suit);
    std::string toString() const;
};

static_assert(sizeof(Card) == 2, "Card is passed by value everywhere from Phase 4 on");

} // namespace romanian_whist

#endif
