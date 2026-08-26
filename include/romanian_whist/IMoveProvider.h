#ifndef IMOVEPROVIDER_H
#define IMOVEPROVIDER_H

#include <romanian_whist/BetContext.h>
#include <romanian_whist/Card.h>
#include <romanian_whist/PlayContext.h>

#include <vector>

namespace romanian_whist
{
class IMoveProvider
{
public:
    virtual ~IMoveProvider() = default;
    virtual unsigned int makeBet(const BetContext& context) = 0;

    // Must return a card from context.hand, not a copy of one: the player
    // removes the played card from the hand by pointer. Returning null says
    // "no legal play", which the caller is entitled to treat as an error.
    virtual Card* playCard(const PlayContext& context) = 0;
};

} // namespace romanian_whist

#endif
