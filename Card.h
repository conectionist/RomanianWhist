#ifndef CARD_H
#define CARD_H

enum class Rank
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

enum class Suit
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
};

#endif
