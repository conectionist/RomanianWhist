#ifndef BET_CONTEXT_H
#define BET_CONTEXT_H

#include <romanian_whist/Card.h>

#include <optional>
#include <vector>

namespace romanian_whist
{
// Everything a player needs in order to choose a bid, gathered into one
// argument so that the bidding seam can grow without breaking every
// IMoveProvider and IStrategy implementation again.
struct BetContext
{
    // The bidder's hand. Its size is the round's trick count, so it doubles as
    // the upper bound on a legal bid.
    const std::vector<Card>& hand;

    // Empty in 8-card rounds, which have no trump.
    std::optional<Card> trump;

    // True for whoever opens the bidding, who bids with nothing to go on.
    bool isFirstPlayer = false;

    // The one bid the final bidder may not make: the value that would bring the
    // round's bids to exactly the trick count. Empty for every other bidder,
    // and empty for the final bidder when the bids so far already exceed the
    // trick count - no single bid can hit the total then.
    std::optional<unsigned int> forbiddenBet;
};

} // namespace romanian_whist

#endif
