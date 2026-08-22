#include "Player.h"

#include <algorithm>
#include <utility>

Player::Player(const string &_name) : name(std::move(_name))
{}

string Player::getName()
{
    return name;
}

void Player::addCardToHand(Card* card)
{
    hand.push_back(card);
}

void Player::clearHand()
{
    hand.clear();
}

const vector<Card*>& Player::getHand() const
{
    return hand;
}

Card* Player::playCard(Card* trump, const Suit* downSuit)
{
    vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, downSuit);

    // TODO: Temporary. Always play the first legal card; no strategy yet.
    Card* card = legalCards[0];
    hand.erase(std::find(hand.begin(), hand.end(), card));
    return card;
}

bool Player::hasSuit(Suit suit) const
{
    for(const auto* card : hand)
    {
        if(card->suit == suit)
            return true;
    }

    return false;
}

unsigned int Player::getBet() const
{
    return 0;
}
