#ifndef PLAYER_H
#define PLAYER_H

#include "Card.h"
#include "CardValidator.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

class Player
{
private:
    string name;
    vector<Card*> hand;
    CardValidator cardValidator;

public:
    Player(const string& _name);
    string getName();
    void addCardToHand(Card* card);
    void clearHand();
    const vector<Card*>& getHand() const;
    Card* playCard(Card* trump, const Suit* downSuit);
    bool hasSuit(Suit suit) const;
    unsigned int getBet() const;
};

#endif
