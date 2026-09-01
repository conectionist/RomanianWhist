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

    // Two cards are the same card when they are the same rank of the same suit.
    // A deck holds no duplicates, so within one game this is identity as well as
    // equality - which is what lets a hand, a trick and a round hold cards by
    // value and still answer "is this the card that was played?".
    bool operator==(const Card&) const = default;
};

static_assert(sizeof(Card) == 2, "Card is exactly a Rank and a Suit; each fits in a byte");

} // namespace romanian_whist

#endif
