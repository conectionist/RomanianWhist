#include <catch2/catch_test_macros.hpp>

#include "GameHarness.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/strategies/DuckingStrategy.h>
#include <romanian_whist/strategies/FirstCardStrategy.h>
#include <romanian_whist/strategies/LowRiskStrategy.h>

#include <memory>
#include <stdexcept>
#include <vector>

using namespace romanian_whist;
using namespace romanian_whist::test;

// A Scoreboard keeps every Round it ever built, but until now there was no way
// to read one back: getCurrentRound() is the only accessor, so a finished round
// was unreachable from outside the engine. "Show me the previous round" is an
// unremarkable thing for a client to want, and it was simply not answerable.
//
// Two things were missing, not one - GameEngine::getRound() to reach the round,
// and Round::getTrick() to reach the cards inside it.

namespace
{
constexpr std::uint32_t kSeed = 42;

std::vector<std::unique_ptr<IMoveProvider>> buildProviders(unsigned int playerCount)
{
    std::vector<std::unique_ptr<IMoveProvider>> providers;

    for(unsigned int i = 0 ; i < playerCount ; i++)
    {
        switch(i % 3)
        {
            case 0:
                providers.push_back(std::make_unique<AiMoveProvider>(std::make_unique<FirstCardStrategy>()));
                break;
            case 1:
                providers.push_back(std::make_unique<AiMoveProvider>(std::make_unique<LowRiskStrategy>()));
                break;
            default:
                providers.push_back(std::make_unique<AiMoveProvider>(std::make_unique<DuckingStrategy>()));
                break;
        }
    }

    return providers;
}
}

TEST_CASE("getRound covers the whole schedule and rejects what is past it", "[history]")
{
    GameEngine engine;
    engine.start(buildSetup(GameStructure::S_818, buildProviders(4), kSeed));

    // The schedule is laid out in full by start(), so every round exists from
    // the outset - the later ones simply have not been played yet.
    REQUIRE(engine.getRoundCount() > 1);
    REQUIRE(engine.getRound(0).getTrickCount() == 8);
    REQUIRE(engine.getRound(engine.getRoundCount() - 1).getPlayedTrickCount() == 0);

    REQUIRE_THROWS_AS(engine.getRound(engine.getRoundCount()), std::out_of_range);
}

TEST_CASE("getRound needs a game that has been started", "[history]")
{
    const GameEngine engine;

    REQUIRE_THROWS_AS(engine.getRound(0), std::logic_error);
}

TEST_CASE("getTrick reaches the cards, and stops at the tricks actually played", "[history]")
{
    const auto engine = playFullGame(GameStructure::S_818, buildProviders(4), kSeed);

    const Round& first = engine->getRound(0);

    REQUIRE(first.getPlayedTrickCount() == first.getTrickCount());
    REQUIRE(first.getTrick(0).getPlayedCards().size() == engine->getPlayerCount());
    REQUIRE(first.getTrick(0).hasWinner());

    REQUIRE_THROWS_AS(first.getTrick(first.getPlayedTrickCount()), std::out_of_range);
}
