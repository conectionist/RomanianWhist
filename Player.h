#ifndef PLAYER_H
#define PLAYER_H

#include <string>

using std::string;

class Player
{
private:
    string name;

public:
    Player(const string& name_);
    string getName();
};

#endif
