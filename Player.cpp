#include "Player.h"

Player::Player(const string &_name) : name(std::move(_name)), next(nullptr)
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

void Player::setNextPlayer(Player *player)
{
    next = player;
}

Player *Player::getNext()
{
    return next;
}

unsigned int Player::getBet() const
{
    return 0;
}
