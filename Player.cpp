#include "Player.h"

Player::Player(const string &_name) : name(std::move(_name))
{}

string Player::getName()
{
    return name;
}

void Player::addCardToHand(const Card* card)
{
    hand.push_back(card);
}

void Player::clearHand()
{
    hand.clear();
}

unsigned int Player::getBet() const
{
    return 0;
}
