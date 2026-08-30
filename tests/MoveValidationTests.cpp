#include <catch2/catch_test_macros.hpp>

#include "ScriptedMoveProvider.h"

#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>

#include <memory>
#include <stdexcept>
#include <string>

using namespace romanian_whist;
using namespace romanian_whist::test;

// Legality used to live entirely in the move providers: AiMoveProvider goes
// through a strategy that calls CardValidator, ConsoleMoveProvider calls it
// directly, and placeBet() recorded whatever it was handed "without judging
// it". That is defensible only while every provider ships in the same repo as
// the rules. The first WebMoveProvider is a thin shim over an untrusted
// browser, and an index arriving over HTTP would reach Round unchecked.
//
// None of the four bundled strategies can trip any of these throws, which is
// why the golden scores do not move - and also why only a scripted provider can
// test that the engine now checks.

namespace
{
// Every seat scripted, so a game is deterministic without depending on which
// strategy happens to do what.
std::unique_ptr<GameEngine> buildGame(std::vector<ScriptedMoveProvider*>& seats,
                                      unsigned int playerCount = 4,
                                      GameStructure structure = GameStructure::S_818)
{
    auto engine = std::make_unique<GameEngine>(7u);

    for(unsigned int i = 0 ; i < playerCount ; i++)
    {
        auto provider = std::make_unique<ScriptedMoveProvider>();
        seats.push_back(provider.get());
        engine->addPlayer("P" + std::to_string(i), std::move(provider));
    }

    engine->initializeScoreboard(structure, false, false);
    engine->initializeDeck(playerCount);
    engine->setStatus(GameStatus::InProgress);

    return engine;
}
}

TEST_CASE("A bid above the trick count is rejected", "[validation]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildGame(seats);

    // S_818's first round deals eight tricks, so nine is impossible.
    REQUIRE(engine->getCurrentRoundTrickCount() == 8);
    seats[0]->forcedIllegalBid = 9;

    REQUIRE_THROWS_AS(engine->run(), std::logic_error);
}

TEST_CASE("The final bidder cannot make the bids add up exactly", "[validation]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildGame(seats);

    // The restriction only ever names a value for the last bidder, and only
    // while a bid could still make the round add up. Read it off the engine at
    // the moment it is asked and hand that exact value back.
    struct BidTheForbiddenValue : IGameObserver
    {
        std::vector<ScriptedMoveProvider*>* seats = nullptr;

        void onBetRequested(const GameEngine& engine, Seat seat) override
        {
            if(const std::optional<unsigned int> forbidden = engine.getForbiddenBet())
                (*seats)[seat.index]->forcedIllegalBid = forbidden;
        }
    };

    BidTheForbiddenValue saboteur;
    saboteur.seats = &seats;
    engine->addObserver(&saboteur);

    REQUIRE_THROWS_AS(engine->run(), std::logic_error);
}

TEST_CASE("A card the player was never dealt is rejected", "[validation]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildGame(seats);

    // Checked by identity against the legal set, so a fabricated card fails
    // however plausible it looks. This is the case a Phase 4 index makes
    // inexpressible rather than merely detectable.
    seats[0]->playCardNotInHand = true;

    REQUIRE_THROWS_AS(engine->run(), std::logic_error);
}

TEST_CASE("A card that fails to follow suit is rejected", "[validation]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildGame(seats);

    // Every seat but the leader tries the first card the rules left out - a
    // card it genuinely holds, which is the half a bounds check would miss.
    // With eight-card hands at four seats, somebody is holding one.
    for(std::size_t i = 1 ; i < seats.size() ; i++)
        seats[i]->playFirstIllegalCard = true;

    REQUIRE_THROWS_AS(engine->run(), std::logic_error);
}

TEST_CASE("A scripted game that plays legally throws nothing", "[validation]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildGame(seats);

    // The control: the same harness, the same seed, nothing sabotaged. Without
    // this the four tests above would still pass on an engine that threw at
    // every move.
    REQUIRE_NOTHROW(engine->run());
    REQUIRE(engine->getStatus() == GameStatus::Finished);
}
