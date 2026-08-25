#include <romanian_whist/Card.h>

namespace romanian_whist
{
Card::Card() : rank(Rank::Two), suit(Suit::Clubs)
{}

Card::Card(Rank _rank, Suit _suit) : rank(_rank), suit(_suit)
{}

std::string Card::toString()
{
    std::string rankString;
    std::string suitString;

    switch(rank)
    {
        case Rank::Two:   rankString = "2"; break;
        case Rank::Three: rankString = "3"; break;
        case Rank::Four:  rankString = "4"; break;
        case Rank::Five:  rankString = "5"; break;
        case Rank::Six:   rankString = "6"; break;
        case Rank::Seven: rankString = "7"; break;
        case Rank::Eight: rankString = "8"; break;
        case Rank::Nine:  rankString = "9"; break;
        case Rank::Ten:   rankString = "10"; break;
        case Rank::Jack:  rankString = "J"; break;
        case Rank::Queen: rankString = "Q"; break;
        case Rank::King:  rankString = "K"; break;
        case Rank::Ace:   rankString = "A"; break;
    }

    switch(suit)
    {
        case Suit::Hearts:   suitString = "H"; break;
        case Suit::Diamonds: suitString = "D"; break;
        case Suit::Spades:   suitString = "S"; break;
        case Suit::Clubs:    suitString = "C"; break;
    }

    return rankString + suitString;
}

} // namespace romanian_whist
