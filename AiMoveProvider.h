#ifndef AI_MOVE_PROVIDER_H
#define AI_MOVE_PROVIDER_H

#include "IMoveProvider.h"
#include "IStrategy.h"
#include <memory>

using std::unique_ptr;

class AiMoveProvider : public IMoveProvider
{
private:
    unique_ptr<IStrategy> strategy;

public:
    AiMoveProvider(unique_ptr<IStrategy> _strategy);
    unsigned int makeBet(const vector<Card*>& hand, Card* trump, bool isFirstPlayer) override;
    Card* playCard(vector<Card*>& hand, Card* trump, const Suit* leadSuit) override;
};

#endif
