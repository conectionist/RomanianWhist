#ifndef SCRIPTED_MOVE_PROVIDER_H
#define SCRIPTED_MOVE_PROVIDER_H

#include <romanian_whist/BetContext.h>
#include <romanian_whist/CardValidator.h>
#include <romanian_whist/IMoveProvider.h>
#include <romanian_whist/PlayContext.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <vector>

namespace romanian_whist::test
{
// A move provider that answers from a script rather than from a strategy.
//
// The bundled strategies are all well behaved by construction - none of them
// can bid a forbidden value or play a card it does not hold - which is exactly
// why they cannot test the engine's new move validation. This can: it is the
// only way to hand the engine a move a real client should never produce.
//
// It is also what drives the cancellation tests, where what matters is that the
// game is deterministic and cheap, not that anyone plays well.
//
// Anything the script does not cover falls back to the first legal card and to
// the lowest legal bid, so a short script still plays a whole game out.
class ScriptedMoveProvider : public IMoveProvider
{
public:
    // Bids, in the order this seat is asked. Exhausted entries fall back below.
    std::vector<unsigned int> bids;

    // Indices into the LEGAL cards for each play, in the order this seat is
    // asked. Legal rather than the whole hand, so a script does not have to
    // know what was dealt to stay legal.
    std::vector<std::size_t> cardChoices;

    // Set to make this provider bid something the round forbids, whatever the
    // script says - the engine should throw before it is ever recorded.
    std::optional<unsigned int> forcedIllegalBid;

    // Set to make this provider play a card it is not allowed to play: the
    // first card of the hand that getLegalCards() left out. Silently does
    // nothing in a trick where every card is legal, which is what the tests
    // using it arrange against.
    bool playFirstIllegalCard = false;

    // Set to name a position past the end of the hand. This is what is left of
    // "return a card the player was never dealt" now that the boundary is an
    // index: a fabricated card can no longer be expressed, so the only bad move
    // a hostile provider can still make is an out-of-range one - and that is a
    // range check rather than a membership search.
    bool playOutOfRangeIndex = false;

    unsigned int makeBet(const BetContext& context) override
    {
        if(forcedIllegalBid)
            return *forcedIllegalBid;

        if(betsMade < bids.size())
            return bids[betsMade++];

        betsMade++;

        // The lowest bid that is allowed. A round always has at least one
        // trick, so stepping 0 up to 1 can never exceed the trick count.
        return context.forbiddenBet && *context.forbiddenBet == 0 ? 1u : 0u;
    }

    std::optional<std::size_t> playCard(const PlayContext& context) override
    {
        if(playOutOfRangeIndex)
            return context.hand.size();

        const CardValidator validator;
        const std::vector<Card> legal = validator.getLegalCards(context.hand,
                                                                context.trump,
                                                                context.leadSuit);

        if(playFirstIllegalCard)
        {
            for(std::size_t i = 0 ; i < context.hand.size() ; i++)
            {
                if(std::find(legal.begin(), legal.end(), context.hand[i]) == legal.end())
                    return i;
            }
        }

        if(legal.empty())
            return std::nullopt;

        const std::size_t choice = cardsPlayed < cardChoices.size() ? cardChoices[cardsPlayed] : 0;

        cardsPlayed++;

        // The script indexes the LEGAL cards, so the choice has to be mapped
        // back onto a hand position before it goes out - the same translation
        // any provider owes when it shows the player a filtered view of the
        // hand. Cards are unique within a deck, so the first match is the only
        // one.
        const Card& picked = legal[choice < legal.size() ? choice : 0];
        const auto it = std::find(context.hand.begin(), context.hand.end(), picked);

        return static_cast<std::size_t>(std::distance(context.hand.begin(), it));
    }

private:
    std::size_t betsMade = 0;
    std::size_t cardsPlayed = 0;
};

} // namespace romanian_whist::test

#endif
