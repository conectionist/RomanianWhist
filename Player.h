#ifndef PLAYER_H
#define PLAYER_H

#include "Card.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

class Player
{
private:
    string name;
    vector<const Card*> hand;
    Player* next;

public:
    Player(const string& _name);
    string getName();
    void addCardToHand(const Card* card);
    void clearHand();
    void setNextPlayer(Player* player);
    Player* getNext();
    unsigned int getBet() const;
};

#endif
