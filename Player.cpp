#include "Player.h"

Player::Player(const string &name_) : name(std::move(name_))
{}

string Player::getName()
{
    return name;
}
