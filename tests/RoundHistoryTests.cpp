#include <catch2/catch_test_macros.hpp>

#include "GameHarness.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>
#include <romanian_whist/strategies/DuckingStrategy.h>
#include <romanian_whist/strategies/FirstCardStrategy.h>
#include <romanian_whist/strategies/LowRiskStrategy.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace romanian_whist;
using namespace romanian_whist::test;

// A Scoreboard keeps every Round it ever built, and a client that renders "the
// previous round" reads them back through GameEngine::getRound(). That only
// means anything if a finished round still describes the game that was actually
// played in it.
//
// It did not, until cards were held by value. Each Round stored its trump and
// its tricks as Card* into the single Deck that GameEngine reshuffles at the
// top of every round, and std::shuffle permutes values between slots - so the
// pointers stayed valid and started denoting different cards. From round two
// onward every finished round misreported itself, and kept changing for the
// rest of the game.
//
// Nothing read history, so nothing caught it. This is that test.

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

// What a round said about itself at the moment it was scored, copied out by
// value so that nothing here shares storage with the engine. Taking the copy is
// the whole technique: the engine's own storage is what moves underneath a
// buggy round, so a snapshot that held pointers would drift along with it and
// agree with itself forever.
struct RoundSnapshot
{
    std::optional<Card> trump;
    std::vector<std::vector<Card>> tricks;

    bool operator==(const RoundSnapshot&) const = default;
};

std::vector<Card> cardsOf(const Trick& trick)
{
    std::vector<Card> cards;

    for(const PlayedCard& played : trick.getPlayedCards())
        cards.push_back(played.card);

    return cards;
}

RoundSnapshot snapshot(const Round& round)
{
    RoundSnapshot taken;

    taken.trump = round.getTrumpCard();

    for(std::size_t i = 0 ; i < round.getPlayedTrickCount() ; i++)
        taken.tricks.push_back(cardsOf(round.getTrick(i)));

    return taken;
}

// Captures every round as it is scored - the last callback at which the round
// being reported is still the current one.
struct HistoryRecorder : IGameObserver
{
    std::vector<RoundSnapshot> asScored;

    void onRoundScored(const GameEngine& engine) override
    {
        asScored.push_back(snapshot(engine.getCurrentRound()));
    }
};
}

TEST_CASE("A finished round still reports the cards it was played with", "[history]")
{
    HistoryRecorder recorder;

    const auto engine = playFullGame(GameStructure::S_818, buildProviders(4), kSeed, { &recorder });

    REQUIRE(recorder.asScored.size() == engine->getRoundCount());

    // Every round, not just the first: the aliasing got worse with each further
    // shuffle, so checking only round 0 would understate it - and checking only
    // the last would miss it entirely, since nothing has reshuffled since.
    for(unsigned int i = 0 ; i < engine->getRoundCount() ; i++)
    {
        const RoundSnapshot afterTheGame = snapshot(engine->getRound(i));
        const RoundSnapshot whenItWasScored = recorder.asScored[i];

        INFO("round " << i);

        REQUIRE(afterTheGame.trump == whenItWasScored.trump);
        REQUIRE(afterTheGame.tricks == whenItWasScored.tricks);
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
