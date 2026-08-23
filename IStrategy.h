#ifndef ISTRATEGY_H
#define ISTRATEGY_H 

#include "Card.h"
#include "CardValidator.h"

#include <vector>

using std::vector;

class IStrategy
{
protected:
    CardValidator cardValidator;
    
public:
    virtual ~IStrategy() = default;
    virtual unsigned int getBestBet(const vector<Card*>& hand, Card* trump, bool isFirstPlayer) = 0;
    virtual Card* getBestChoice(vector<Card*>& hand, Card* trump, const Suit* downSuit) = 0;
};

#endif
