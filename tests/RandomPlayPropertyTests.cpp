#include <catch2/catch_test_macros.hpp>

#include "GameHarness.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/CardValidator.h>
#include <romanian_whist/strategies/RandomCardStrategy.h>

#include <algorithm>
#include <numeric>

using namespace romanian_whist;
using namespace romanian_whist::test;

namespace
{
// Fixed, committed range - not std::random_device - so a failure reproduces
// locally with the seed printed via INFO below. See ENGINE_V4_PLAN.md
// section 0g: breadth across seeds is the point, not which seeds.
constexpr std::uint32_t kPropertySeedCount = 2000;
}

TEST_CASE("Random play never violates the rules", "[property]")
{
    CardValidator validator;

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
            const auto legal = validator.getLegalCards(handBeforePlay, trump, leadSuit);
            REQUIRE(std::find(legal.begin(), legal.end(), playedCard) != legal.end());
        };

        hooks.onRoundScored = [&](const GameEngine& engine)
        {
            const auto& players = engine.getPlayers();
            unsigned int totalTricksWon = 0;
            for(unsigned int i = 0 ; i < players.size() ; i++)
                totalTricksWon += engine.getCurrentRound().getActual(players.at(i).getName());

            REQUIRE(totalTricksWon == engine.getCurrentRoundTrickCount());
        };

        playFullGame(structure, std::move(providers), seed, hooks);
    }
}
