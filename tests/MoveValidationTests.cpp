#include <catch2/catch_test_macros.hpp>

#include "GameHarness.h"
#include "ScriptedMoveProvider.h"

#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>
#include <romanian_whist/Player.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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
    std::vector<std::unique_ptr<IMoveProvider>> providers;

    for(unsigned int i = 0 ; i < playerCount ; i++)
    {
        auto provider = std::make_unique<ScriptedMoveProvider>();
        seats.push_back(provider.get());
        providers.push_back(std::move(provider));
    }

    auto engine = std::make_unique<GameEngine>();
    engine->start(test::buildSetup(structure, std::move(providers), 7u));

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

TEST_CASE("A card index outside the hand is rejected", "[validation]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildGame(seats);

    // This used to be "a card the player was never dealt", tested by handing
    // the engine a fabricated Card and watching it fail a membership check.
    // That move no longer exists: the boundary is an index into the hand, so a
    // card the player does not hold cannot be named at all, and the only
    // remaining bad answer is a position past the end of the hand.
    //
    // The check that replaced it is strictly stronger - a range check against a
    // hand the engine already holds, rather than a search a hostile caller
    // could hope to satisfy - and it is the whole point of the index boundary.
    seats[0]->playOutOfRangeIndex = true;

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

TEST_CASE("Playing a card removes exactly that card, and nothing else moves", "[validation]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildGame(seats);

    // The invariant the index boundary creates, and the one thing it could get
    // wrong that a card-returning boundary could not: the engine now erases by
    // POSITION, so an off-by-one would quietly play a different card than the
    // one the provider chose. Nothing else in the suite would notice - the card
    // erased would still be a legal card from a legal hand.
    struct HandWatcher : IGameObserver
    {
        std::vector<Card> before;
        Seat watched{0};

        void onCardRequested(const GameEngine& engine, Seat seat) override
        {
            watched = seat;
            before = engine.getPlayers().at(seat.index).getHand();
        }

        void onCardPlayed(const GameEngine& engine, Seat seat, const Card& card) override
        {
            REQUIRE(seat == watched);

            const std::vector<Card>& after = engine.getPlayers().at(seat.index).getHand();

            REQUIRE(after.size() + 1 == before.size());

            // Rebuild what the hand should now be by dropping the first copy of
            // the played card, and require the survivors kept their order. A
            // hand that merely still *contains* the right cards would pass a
            // weaker check while having been shuffled underneath the player.
            std::vector<Card> expected = before;
            const auto it = std::find(expected.begin(), expected.end(), card);

            REQUIRE(it != expected.end());
            expected.erase(it);

            REQUIRE(after == expected);
        }
    };

    HandWatcher watcher;
    engine->addObserver(&watcher);

    REQUIRE_NOTHROW(engine->run());
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
