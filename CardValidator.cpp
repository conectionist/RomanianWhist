#include "CardValidator.h"

vector<Card*> CardValidator::getLegalCards(const vector<Card*>& hand, Card* trump, const Suit* downSuit) const
{
    vector<Card*> legalCards;

    if(hand.empty())
        return legalCards;

    if(downSuit == nullptr)
        return hand;

    const bool mustFollowSuit = hasSuit(hand, *downSuit);
    const bool mustPlayTrump = !mustFollowSuit && trump && hasSuit(hand, trump->suit);

    for(Card* card : hand)
    {
        if(mustFollowSuit)
        {
            if(card->suit == *downSuit)
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

bool CardValidator::hasSuit(const vector<Card*>& hand, Suit suit) const
{
    for(const auto* card : hand)
    {
        if(card->suit == suit)
            return true;
    }

    return false;
}
