#ifndef CONSOLE_MOVE_PROVIDER_H
#define CONSOLE_MOVE_PROVIDER_H

#include "IMoveProvider.h"

class ConsoleMoveProvider : public IMoveProvider
{
public:
    ConsoleMoveProvider() = default;
    virtual ~ConsoleMoveProvider() = default;
    virtual unsigned int makeBet(const vector<Card*>& hand, Card* trump, bool isFirstPlayer) override;
    virtual Card* playCard(vector<Card*>& hand, Card* trump, const Suit* leadSuit) override;
};

#endif