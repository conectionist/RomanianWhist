#include <catch2/catch_test_macros.hpp>

#include "GameHarness.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/strategies/RandomCardStrategy.h>

#include <algorithm>

using namespace romanian_whist;
using namespace romanian_whist::test;

namespace
{
// Fixed, committed range - not std::random_device - so a failure reproduces
// locally with the seed printed via INFO below. Breadth across seeds is the
// point here, not which seeds are used.
constexpr std::uint32_t kPropertySeedCount = 2000;

// Independently reimplements the "follow suit, else trump, else anything"
// rule, rather than calling CardValidator::getLegalCards() - the strategy
// under test already calls that function to choose its card, so re-deriving
// "expected" from the very function that produced "actual" would make this
// check pass regardless of whether that function is correct.
bool followsTheRules(const std::vector<Card*>& hand, Card* trump, const Suit* leadSuit, Card* playedCard)
{
    if(std::find(hand.begin(), hand.end(), playedCard) == hand.end())
        return false;

    if(leadSuit == nullptr)
        return true;

    const auto holdsSuit = [&](Suit suit)
    {
        return std::any_of(hand.begin(), hand.end(), [suit](Card* card) { return card->suit == suit; });
    };

    if(holdsSuit(*leadSuit))
        return playedCard->suit == *leadSuit;

    if(trump && holdsSuit(trump->suit))
        return playedCard->suit == trump->suit;

    return true;
}
}

TEST_CASE("Random play never violates the rules", "[property]")
{
    for(std::uint32_t seed = 0 ; seed < kPropertySeedCount ; seed++)
    {
        INFO("seed = " << seed);

        const unsigned int playerCount = 2 + (seed % 5);
        const GameStructure structure = (seed % 2 == 0) ? GameStructure::S_181 : GameStructure::S_818;

        std::vector<std::unique_ptr<IMoveProvider>> providers;
        for(unsigned int seat = 0 ; seat < playerCount ; seat++)
        {
            const std::uint32_t providerSeed = seed * playerCount + seat;
            providers.push_back(std::make_unique<AiMoveProvider>(
                std::make_unique<RandomCardStrategy>(providerSeed)));
        }

        GameHooks hooks;

        hooks.onBeforeBetPlaced = [&](const GameEngine& engine, unsigned int, unsigned int bet)
        {
            REQUIRE(engine.isBetLegal(bet));
        };

        hooks.onBeforeCardPlayed = [&](const std::vector<Card*>& handBeforePlay, Card* trump,
                                       const Suit* leadSuit, Card* playedCard)
        {
            REQUIRE(followsTheRules(handBeforePlay, trump, leadSuit, playedCard));
        };

        hooks.onRoundScored = [&](const GameEngine& engine)
        {
            // Every trick is stored with its winner and tricks-won is counted
            // off that, so summing it across the seats would only recount the
            // stored tricks. Assert the thing that still has teeth: the round
            // played out exactly as many tricks as it was dealt for.
            REQUIRE(engine.getCurrentRound().getPlayedTrickCount() ==
                    engine.getCurrentRoundTrickCount());
        };

        playFullGame(structure, std::move(providers), seed, hooks);
    }
}
