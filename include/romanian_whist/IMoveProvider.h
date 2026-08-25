#ifndef IMOVEPROVIDER_H
#define IMOVEPROVIDER_H

#include <romanian_whist/BetContext.h>
#include <romanian_whist/Card.h>

#include <vector>

namespace romanian_whist
{
class IMoveProvider
{
public:
    virtual ~IMoveProvider() = default;
    virtual unsigned int makeBet(const BetContext& context) = 0;
    virtual Card* playCard(const std::vector<Card*>& hand, Card* trump, const Suit* leadSuit) = 0;
};

} // namespace romanian_whist

#endif
