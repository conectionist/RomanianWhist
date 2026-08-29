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

        hooks.onBeforeBetPlaced = [&](const GameEngine& engine, Seat, unsigned int bet)
        {
            REQUIRE(engine.isBetLegal(bet));
        };

        hooks.onBeforeCardPlayed = [&](const std::vector<Card*>& handBeforePlay, Card* trump,
                                       const Suit* leadSuit, Card* playedCard)
        {
            REQUIRE(followsTheRules(handBeforePlay, trump, leadSuit, playedCard));
        };

        // Tallied here as each trick is won, so the round's own results have
        // something independent to be held against. A sum over the seats would
        // not do: addTrick() rejects a trick with no winner or an off-table
        // one, so that total can only ever equal getPlayedTrickCount(), and it
        // stays right even if the round credits every trick to the wrong seat.
        std::vector<unsigned int> tricksWonBySeat(playerCount, 0);

        hooks.onTrickWon = [&](Seat winner)
        {
            tricksWonBySeat[winner.index]++;
        };

        hooks.onRoundScored = [&](const GameEngine& engine)
        {
            const Round& round = engine.getCurrentRound();

            for(unsigned int i = 0 ; i < engine.getPlayerCount() ; i++)
                REQUIRE(round.getTricksWon(Seat{i}) == tricksWonBySeat[i]);

            REQUIRE(round.getPlayedTrickCount() == engine.getCurrentRoundTrickCount());

            std::fill(tricksWonBySeat.begin(), tricksWonBySeat.end(), 0u);
        };

        playFullGame(structure, std::move(providers), seed, hooks);
    }
}
