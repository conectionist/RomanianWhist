#include <romanian_whist/CardValidator.h>

namespace romanian_whist
{
std::vector<Card*> CardValidator::getLegalCards(const std::vector<Card*>& hand, Card* trump, const Suit* leadSuit) const
{
    std::vector<Card*> legalCards;

    if(hand.empty())
        return legalCards;

    if(leadSuit == nullptr)
        return hand;

    const bool mustFollowSuit = hasSuit(hand, *leadSuit);
    const bool mustPlayTrump = !mustFollowSuit && trump && hasSuit(hand, trump->suit);

    for(Card* card : hand)
    {
        if(mustFollowSuit)
        {
            if(card->suit == *leadSuit)
                legalCards.push_back(card);
        }
        else if(mustPlayTrump)
        {
            if(card->suit == trump->suit)
                legalCards.push_back(card);
        }
        else
        {
            legalCards.push_back(card);
        }
    }

    return legalCards;
}

bool CardValidator::hasSuit(const std::vector<Card*>& hand, Suit suit) const
{
    for(const auto* card : hand)
    {
        if(card->suit == suit)
            return true;
    }

    return false;
}

} // namespace romanian_whist
