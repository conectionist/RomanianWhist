#include <catch2/catch_test_macros.hpp>

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>
#include <romanian_whist/strategies/FirstCardStrategy.h>

#include "ScriptedMoveProvider.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace romanian_whist;
using namespace romanian_whist::test;

namespace
{
std::unique_ptr<IMoveProvider> dummyProvider()
{
    return std::make_unique<AiMoveProvider>(std::make_unique<FirstCardStrategy>());
}

// Counts what it is told, and records what the engine looked like when it was
// told - the callbacks are only worth anything if the state visible inside them
// is the state IGameObserver.h promises.
struct CountingObserver : IGameObserver
{
    unsigned int gameStarted = 0;

    // Engine state as seen from inside onGameStarted().
    GamePhase phaseAtStart = GamePhase::GameOver;
    GameStatus statusAtStart = GameStatus::NotStarted;
    unsigned int roundCountAtStart = 0;

    void onGameStarted(const GameEngine& engine) override
    {
        gameStarted++;
        phaseAtStart = engine.getPhase();
        statusAtStart = engine.getStatus();
        roundCountAtStart = engine.getRoundCount();
    }
};

// By pointer: GameEngine holds an atomic stop flag, so it is neither copyable
// nor movable and cannot be returned by value.
std::unique_ptr<GameEngine> buildEngine(unsigned int playerCount)
{
    auto engine = std::make_unique<GameEngine>(1u);

    for(unsigned int i = 0 ; i < playerCount ; i++)
        engine->addPlayer("P" + std::to_string(i), dummyProvider());

    engine->initializeScoreboard(GameStructure::S_181, false, false);
    engine->initializeDeck(playerCount);

    return engine;
}
}

TEST_CASE("GameEngine fires onGameStarted when the game starts", "[observer]")
{
    auto enginePtr = buildEngine(3);
    GameEngine& engine = *enginePtr;
    CountingObserver observer;
    engine.addObserver(&observer);

    REQUIRE(observer.gameStarted == 0);
    REQUIRE(engine.getPhase() == GamePhase::NotStarted);

    engine.setStatus(GameStatus::InProgress);

    REQUIRE(observer.gameStarted == 1);

    // The schedule exists and the status is live by the time an observer is
    // told, so a client can read the game's shape from inside the callback.
    REQUIRE(observer.statusAtStart == GameStatus::InProgress);
    REQUIRE(observer.roundCountAtStart == engine.getRoundCount());

    // But nothing has been dealt, so the round has not begun: the phase is
    // still NotStarted until the first deal moves it to Betting.
    REQUIRE(observer.phaseAtStart == GamePhase::NotStarted);

    SECTION("and only once, however often InProgress is set again")
    {
        engine.setStatus(GameStatus::InProgress);
        engine.setStatus(GameStatus::InProgress);

        REQUIRE(observer.gameStarted == 1);
    }

    SECTION("and a finished game cannot be restarted into a second one")
    {
        engine.setStatus(GameStatus::Finished);

        // Finished is terminal. This transition used to be allowed and merely
        // not re-notify - which left the engine InProgress on a schedule parked
        // at its last round, so run() re-dealt that round and died inside
        // Round::addTrick rather than here.
        REQUIRE_THROWS_AS(engine.setStatus(GameStatus::InProgress), std::logic_error);

        REQUIRE(engine.getStatus() == GameStatus::Finished);
        REQUIRE(observer.gameStarted == 1);
    }

    SECTION("and a started game cannot be returned to NotStarted")
    {
        engine.setStatus(GameStatus::InProgress);

        // Otherwise the next move back to InProgress reads as a first start and
        // fires onGameStarted() again, on a game that is already part-played.
        REQUIRE_THROWS_AS(engine.setStatus(GameStatus::NotStarted), std::logic_error);

        REQUIRE(engine.getStatus() == GameStatus::InProgress);
        REQUIRE(observer.gameStarted == 1);
    }
}

TEST_CASE("GameEngine::addObserver ignores a repeat registration", "[observer]")
{
    auto enginePtr = buildEngine(3);
    GameEngine& engine = *enginePtr;
    CountingObserver observer;

    engine.addObserver(&observer);
    engine.addObserver(&observer);
    engine.addObserver(&observer);

    engine.setStatus(GameStatus::InProgress);

    // Registered three times, told once - otherwise a client that registers
    // defensively would render every frame three times over.
    REQUIRE(observer.gameStarted == 1);
}

TEST_CASE("GameEngine::removeObserver stops the callbacks", "[observer]")
{
    auto enginePtr = buildEngine(3);
    GameEngine& engine = *enginePtr;
    CountingObserver observer;

    engine.addObserver(&observer);
    engine.removeObserver(&observer);
    engine.setStatus(GameStatus::InProgress);

    REQUIRE(observer.gameStarted == 0);

    // Removing one that was never registered is not an error - there is nothing
    // for a client to have got wrong.
    REQUIRE_NOTHROW(engine.removeObserver(&observer));
}

TEST_CASE("GameEngine::addObserver rejects null", "[observer]")
{
    auto enginePtr = buildEngine(3);
    GameEngine& engine = *enginePtr;

    // Stored, this would be dereferenced at the first callback, a long way from
    // the call that got it wrong.
    REQUIRE_THROWS_AS(engine.addObserver(nullptr), std::invalid_argument);
}

TEST_CASE("GameEngine dispatches to every registered observer", "[observer]")
{
    auto enginePtr = buildEngine(3);
    GameEngine& engine = *enginePtr;
    CountingObserver first;
    CountingObserver second;

    engine.addObserver(&first);
    engine.addObserver(&second);
    engine.setStatus(GameStatus::InProgress);

    REQUIRE(first.gameStarted == 1);
    REQUIRE(second.gameStarted == 1);
}

// ---------------------------------------------------------------------------
// The loop as seen from outside. IGameObserver.h promises not just an order of
// callbacks but a particular engine state visible inside each one, and those
// promises are what the clients are written against - a client that reads the
// wrong one draws a correct game wrongly, which no golden test can catch.

namespace
{
std::unique_ptr<GameEngine> buildScriptedGame(unsigned int playerCount, GameStructure structure)
{
    auto engine = std::make_unique<GameEngine>(5u);

    for(unsigned int i = 0 ; i < playerCount ; i++)
        engine->addPlayer("P" + std::to_string(i), std::make_unique<ScriptedMoveProvider>());

    engine->initializeScoreboard(structure, false, false);
    engine->initializeDeck(playerCount);
    engine->setStatus(GameStatus::InProgress);

    return engine;
}
}

TEST_CASE("The callbacks fire in the documented order", "[observer]")
{
    const auto engine = buildScriptedGame(3, GameStructure::S_181);

    struct Recorder : IGameObserver
    {
        std::vector<std::string> events;

        void onRoundStarted(const GameEngine&) override      { events.push_back("roundStarted"); }
        void onBetRequested(const GameEngine&, Seat) override { events.push_back("betRequested"); }
        void onBetPlaced(const GameEngine&, Seat, unsigned int) override { events.push_back("betPlaced"); }
        void onBettingComplete(const GameEngine&) override    { events.push_back("bettingComplete"); }
        void onTrickStarted(const GameEngine&, unsigned int, Seat) override { events.push_back("trickStarted"); }
        void onCardRequested(const GameEngine&, Seat) override { events.push_back("cardRequested"); }
        void onCardPlayed(const GameEngine&, Seat, const Card&) override { events.push_back("cardPlayed"); }
        void onTrickWon(const GameEngine&, Seat, unsigned int) override { events.push_back("trickWon"); }
        void onRoundScored(const GameEngine&) override        { events.push_back("roundScored"); }
        void onRoundComplete(const GameEngine&) override      { events.push_back("roundComplete"); }
        void onGameOver(const GameEngine&) override           { events.push_back("gameOver"); }
    };

    Recorder recorder;
    engine->addObserver(&recorder);
    engine->run();

    // The first round of a 3-player S_181 schedule is a single trick, so its
    // whole event sequence is short enough to write out in full.
    const std::vector<std::string> expectedFirstRound{
        "roundStarted",
        "betRequested", "betPlaced",
        "betRequested", "betPlaced",
        "betRequested", "betPlaced",
        "bettingComplete",
        "trickStarted",
        "cardRequested", "cardPlayed",
        "cardRequested", "cardPlayed",
        "cardRequested", "cardPlayed",
        "trickWon",
        "roundScored",
        "roundComplete",
    };

    const std::vector<std::string> firstRound(recorder.events.begin(),
                                              recorder.events.begin()
                                                  + static_cast<long>(expectedFirstRound.size()));

    REQUIRE(firstRound == expectedFirstRound);

    // And the game ends with exactly one gameOver, after the last round has
    // been reported complete.
    REQUIRE(recorder.events.back() == "gameOver");
    REQUIRE(std::count(recorder.events.begin(), recorder.events.end(), "gameOver") == 1);
    REQUIRE(recorder.events[recorder.events.size() - 2] == "roundComplete");
}

TEST_CASE("Each callback sees the state IGameObserver promises", "[observer]")
{
    const auto engine = buildScriptedGame(4, GameStructure::S_818);

    struct Checker : IGameObserver
    {
        Seat roundLeader{0};
        Seat trickLeader{0};
        unsigned int roundIndexAtScored = 0;
        unsigned int roundsChecked = 0;
        unsigned int tricksChecked = 0;

        void onRoundStarted(const GameEngine& engine) override
        {
            roundLeader = engine.getRoundLeaderSeat();

            // Dealt, but nothing played: the in-flight trick is empty even
            // though the previous round ended holding its last one.
            REQUIRE(engine.getPhase() == GamePhase::Betting);
            REQUIRE(engine.getCurrentTrickNumber() == 0);
            REQUIRE(engine.getCurrentTrick().getPlayedCards().empty());
            REQUIRE_FALSE(engine.getCurrentTrickLeader().has_value());
            REQUIRE_FALSE(engine.getActiveSeat().has_value());

            // Nobody has bid yet.
            for(unsigned int i = 0 ; i < engine.getPlayerCount() ; i++)
                REQUIRE_FALSE(engine.getBet(Seat{i}).has_value());
        }

        void onBetRequested(const GameEngine& engine, Seat seat) override
        {
            REQUIRE(engine.getActiveSeat() == seat);
            REQUIRE_FALSE(engine.getBet(seat).has_value());
        }

        void onBetPlaced(const GameEngine& engine, Seat seat, unsigned int bet) override
        {
            // Still that seat's turn as far as a renderer is concerned, which
            // is what keeps the highlight on whoever just moved.
            REQUIRE(engine.getActiveSeat() == seat);
            REQUIRE(engine.getBet(seat) == bet);
        }

        void onBettingComplete(const GameEngine& engine) override
        {
            REQUIRE(engine.getPhase() == GamePhase::Playing);
            REQUIRE_FALSE(engine.getActiveSeat().has_value());

            for(unsigned int i = 0 ; i < engine.getPlayerCount() ; i++)
                REQUIRE(engine.getBet(Seat{i}).has_value());
        }

        void onTrickStarted(const GameEngine& engine, unsigned int trickNumber, Seat leader) override
        {
            trickLeader = leader;

            REQUIRE(engine.getCurrentTrickNumber() == trickNumber);
            REQUIRE(engine.getCurrentTrick().getPlayedCards().empty());
            REQUIRE(engine.getTrickLeaderSeat() == leader);

            // The round leader is fixed for the round; the trick leader is not.
            REQUIRE(engine.getRoundLeaderSeat() == roundLeader);

            if(trickNumber == 1)
                REQUIRE(leader == roundLeader);
        }

        void onCardRequested(const GameEngine& engine, Seat seat) override
        {
            REQUIRE(engine.getActiveSeat() == seat);

            // This seat's card is not in the trick yet, so the highlight names
            // whoever is winning without it.
            for(const PlayedCard& played : engine.getCurrentTrick().getPlayedCards())
                REQUIRE(played.seat != seat);
        }

        void onCardPlayed(const GameEngine& engine, Seat seat, const Card& card) override
        {
            REQUIRE(engine.getActiveSeat() == seat);

            const std::vector<PlayedCard>& played = engine.getCurrentTrick().getPlayedCards();

            REQUIRE_FALSE(played.empty());
            REQUIRE(played.back().seat == seat);
            REQUIRE(played.back().card == &card);

            // The first card of the trick was played by its leader, and it is
            // the one setting the suit everyone else must follow.
            REQUIRE(played.front().seat == trickLeader);
            REQUIRE(engine.getCurrentTrick().hasLeadSuit());
            REQUIRE(engine.getCurrentTrick().getLeadSuit() == played.front().card->suit);

            // Somebody is winning as soon as a card is down.
            REQUIRE(engine.getCurrentTrickLeader().has_value());
        }

        void onTrickWon(const GameEngine& engine, Seat winner, unsigned int trickNumber) override
        {
            // The finished trick is still readable - a client draws the
            // completed table under "X wins the trick" from exactly here.
            const std::vector<PlayedCard>& played = engine.getCurrentTrick().getPlayedCards();
            REQUIRE(played.size() == engine.getPlayerCount());
            REQUIRE(engine.getCurrentTrickNumber() == trickNumber);

            // And it is still the trick that was just played, not the next one:
            // its first entry names who led it, which is what a client must
            // rebuild its table from.
            REQUIRE(played.front().seat == trickLeader);

            // The trick has already been filed, so the winner's count includes
            // it. An observer redrawing the scoreboard here would otherwise be
            // one short, every trick.
            REQUIRE(engine.getTricksWon(winner) >= 1);
            REQUIRE(engine.getCurrentTrickLeader() == winner);

            // Meanwhile getTrickLeaderSeat() has ALREADY moved on to the
            // winner - which is exactly why a client must not rebuild its table
            // as (leader + i) % playerCount from inside this callback.
            REQUIRE(engine.getTrickLeaderSeat() == winner);
            REQUIRE(engine.getRoundLeaderSeat() == roundLeader);

            REQUIRE_FALSE(engine.getActiveSeat().has_value());

            unsigned int total = 0;
            for(unsigned int i = 0 ; i < engine.getPlayerCount() ; i++)
                total += engine.getTricksWon(Seat{i});

            REQUIRE(total == trickNumber);

            tricksChecked++;
        }

        void onRoundScored(const GameEngine& engine) override
        {
            // The last callback at which the index still names the round being
            // reported. A client drawing "round N of M" from onRoundComplete
            // draws the next round's number over this round's scores.
            roundIndexAtScored = engine.getCurrentRoundIndex();

            REQUIRE(engine.getPhase() == GamePhase::RoundScored);
            REQUIRE(engine.getCurrentRound().getPlayedTrickCount()
                    == engine.getCurrentRoundTrickCount());
        }

        void onRoundComplete(const GameEngine& engine) override
        {
            if(engine.getStatus() == GameStatus::Finished)
            {
                // The last round is the exception: there is no next round to
                // advance to, so the index stays where it was.
                REQUIRE(engine.getCurrentRoundIndex() == roundIndexAtScored);
                REQUIRE(engine.getPhase() == GamePhase::GameOver);
            }
            else
            {
                REQUIRE(engine.getCurrentRoundIndex() == roundIndexAtScored + 1);
            }

            roundsChecked++;
        }
    };

    Checker checker;
    engine->addObserver(&checker);
    engine->run();

    REQUIRE(checker.roundsChecked == engine->getRoundCount());
    REQUIRE(checker.tricksChecked > 0);
}
