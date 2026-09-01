#ifndef IMOVEPROVIDER_H
#define IMOVEPROVIDER_H

#include <romanian_whist/BetContext.h>
#include <romanian_whist/Card.h>
#include <romanian_whist/PlayContext.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace romanian_whist
{
class IMoveProvider
{
public:
    virtual ~IMoveProvider() = default;
    virtual unsigned int makeBet(const BetContext& context) = 0;

    // The position of the chosen card WITHIN context.hand. Empty says "no legal
    // play", which the caller is entitled to treat as an error.
    //
    // An index rather than a card, because an index cannot name a card the
    // player does not hold: the engine range-checks it and erases by position,
    // so a fabricated card is not merely rejected, it cannot be expressed. That
    // matters most for a provider that is a thin shim over an untrusted client.
    //
    // Note this is an index into the hand as given, NOT into any reordered or
    // filtered view of it. A provider that shows the player a sorted or
    // legal-only list owes the translation back; see ConsoleMoveProvider.
    virtual std::optional<std::size_t> playCard(const PlayContext& context) = 0;
};

} // namespace romanian_whist

#endif
