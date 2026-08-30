#ifndef SCRIPTED_MOVE_PROVIDER_H
#define SCRIPTED_MOVE_PROVIDER_H

#include <romanian_whist/BetContext.h>
#include <romanian_whist/CardValidator.h>
#include <romanian_whist/IMoveProvider.h>
#include <romanian_whist/PlayContext.h>

#include <cstddef>
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

    // Set to return a card this player was never dealt. Unlike the flag above
    // this always fires, whatever was dealt - the engine checks membership by
    // pointer, and this one points outside the deck entirely.
    bool playCardNotInHand = false;

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

    Card* playCard(const PlayContext& context) override
    {
        if(playCardNotInHand)
        {
            static Card fabricated(Rank::Ace, Suit::Spades);
            return &fabricated;
        }

        const CardValidator validator;
        const std::vector<Card*> legal = validator.getLegalCards(context.hand,
                                                                 context.trump,
                                                                 context.leadSuit);

        if(playFirstIllegalCard)
        {
            for(Card* card : context.hand)
            {
                if(std::find(legal.begin(), legal.end(), card) == legal.end())
                    return card;
            }
        }

        if(legal.empty())
            return nullptr;

        const std::size_t choice = cardsPlayed < cardChoices.size() ? cardChoices[cardsPlayed] : 0;

        cardsPlayed++;

        return legal[choice < legal.size() ? choice : 0];
    }

private:
    std::size_t betsMade = 0;
    std::size_t cardsPlayed = 0;
};

} // namespace romanian_whist::test

#endif
