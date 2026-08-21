#include "Player.h"

Player::Player(const string &_name) : name(std::move(_name))
{}

string Player::getName()
{
    return name;
}
