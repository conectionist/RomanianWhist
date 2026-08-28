#include <catch2/catch_test_macros.hpp>

#include "GameHarness.h"
#include "TestSupport.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/strategies/DuckingStrategy.h>
#include <romanian_whist/strategies/FirstCardStrategy.h>
#include <romanian_whist/strategies/LowRiskStrategy.h>

using namespace romanian_whist;
using namespace romanian_whist::test;

namespace
{
constexpr std::uint32_t kGoldenSeed = 42;

// Round-robins the three deterministic strategies across seats, so every
// scenario exercises all three regardless of player count.
// RandomCardStrategy is deliberately excluded: its draw count depends on
// what every other seat bids, so any behavioural change desyncs its stream
// and turns a located failure into "something changed somewhere" instead.
std::vector<std::unique_ptr<IMoveProvider>> buildRoundRobinProviders(unsigned int playerCount)
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

TEST_CASE("Golden game: 2 players, S_181", "[golden]")
{
    GameEngine engine = playFullGame(GameStructure::S_181, buildRoundRobinProviders(2), kGoldenSeed);
    REQUIRE(finalScores(engine) == std::vector<int>{ -36, 28 });
}

TEST_CASE("Golden game: 2 players, S_818", "[golden]")
{
    GameEngine engine = playFullGame(GameStructure::S_818, buildRoundRobinProviders(2), kGoldenSeed);
    REQUIRE(finalScores(engine) == std::vector<int>{ -19, 4 });
}

TEST_CASE("Golden game: 4 players, S_181, with full round record", "[golden]")
{
    RoundRecord record;

    GameHooks hooks;
    hooks.onRoundScored = recordRoundsInto(record);

    GameEngine engine = playFullGame(GameStructure::S_181, buildRoundRobinProviders(4), kGoldenSeed, hooks);

    REQUIRE(finalScores(engine) == std::vector<int>{ 39, 56, 10, 16 });
    REQUIRE(record.size() == engine.getRoundCount());

    // clang-format off
    const RoundRecord expected{
        { {0,0}, {0,0}, {0,0}, {0,1} },
        { {1,0}, {1,1}, {0,0}, {0,0} },
        { {0,0}, {0,0}, {0,0}, {0,1} },
        { {0,0}, {0,0}, {0,1}, {0,0} },
        { {0,0}, {0,0}, {0,0}, {0,2} },
        { {0,0}, {1,0}, {0,1}, {0,2} },
        { {0,1}, {1,1}, {0,2}, {0,0} },
        { {0,0}, {1,4}, {0,1}, {0,0} },
        { {0,3}, {1,0}, {0,1}, {0,2} },
        { {0,2}, {0,2}, {0,2}, {0,1} },
        { {0,1}, {1,1}, {0,6}, {0,0} },
        { {0,2}, {0,2}, {0,0}, {0,4} },
        { {0,1}, {1,4}, {0,2}, {0,1} },
        { {0,1}, {2,6}, {0,0}, {0,1} },
        { {0,0}, {1,1}, {0,2}, {0,4} },
        { {0,1}, {0,2}, {0,1}, {0,2} },
        { {0,0}, {2,3}, {0,1}, {0,1} },
        { {0,2}, {1,1}, {0,0}, {0,1} },
        { {0,1}, {1,1}, {0,1}, {0,0} },
        { {0,0}, {0,0}, {0,2}, {0,0} },
        { {0,0}, {0,0}, {0,0}, {0,1} },
        { {0,0}, {0,0}, {0,1}, {0,0} },
        { {0,0}, {0,0}, {0,1}, {0,0} },
        { {0,0}, {0,1}, {0,0}, {0,0} },
    };
    // clang-format on

    REQUIRE(record == expected);
}

TEST_CASE("Golden game: 4 players, S_818", "[golden]")
{
    GameEngine engine = playFullGame(GameStructure::S_818, buildRoundRobinProviders(4), kGoldenSeed);
    REQUIRE(finalScores(engine) == std::vector<int>{ -6, 48, 20, -25 });
}

TEST_CASE("Golden game: 6 players, S_181", "[golden]")
{
    GameEngine engine = playFullGame(GameStructure::S_181, buildRoundRobinProviders(6), kGoldenSeed);
    REQUIRE(finalScores(engine) == std::vector<int>{ -8, 78, 55, 82, 75, 114 });
}

TEST_CASE("Golden game: 6 players, S_818", "[golden]")
{
    GameEngine engine = playFullGame(GameStructure::S_818, buildRoundRobinProviders(6), kGoldenSeed);
    REQUIRE(finalScores(engine) == std::vector<int>{ 47, 72, 46, 51, 119, 58 });
}
