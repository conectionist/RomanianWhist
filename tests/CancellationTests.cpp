#include <catch2/catch_test_macros.hpp>

#include "ScriptedMoveProvider.h"

#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>

#include <memory>
#include <stdexcept>
#include <string>

using namespace romanian_whist;
using namespace romanian_whist::test;

// requestStop() is exercised by nothing else in the suite: the golden games and
// the property games all run to completion, and the terminal's manual play
// check covers pacing rather than cancellation. A boundary-checked flag is
// exactly the kind of thing a tidy-up shifts by one trick without anything else
// noticing, so pin it here.

namespace
{
struct Tally : IGameObserver
{
    unsigned int roundsStarted = 0;
    unsigned int roundsScored = 0;
    unsigned int tricksWon = 0;
    unsigned int gameOver = 0;
    unsigned int gameStopped = 0;

    // Set to stop the game from inside a callback, which is the closest a
    // single-threaded test gets to a click arriving mid-game.
    GameEngine* stopAfterTrick = nullptr;
    unsigned int stopAfterTrickNumber = 0;

    void onRoundStarted(const GameEngine&) override { roundsStarted++; }
    void onRoundScored(const GameEngine&) override { roundsScored++; }
    void onGameOver(const GameEngine&) override { gameOver++; }
    void onGameStopped(const GameEngine&) override { gameStopped++; }

    void onTrickWon(const GameEngine&, Seat, unsigned int trickNumber) override
    {
        tricksWon++;

        if(stopAfterTrick != nullptr && trickNumber == stopAfterTrickNumber)
            stopAfterTrick->requestStop();
    }
};

// A 4-player S_818 game, so the very first round has eight tricks and there is
// room to stop partway through one.
std::unique_ptr<GameEngine> buildGame(GameStructure structure = GameStructure::S_818)
{
    auto engine = std::make_unique<GameEngine>(7u);

    for(unsigned int i = 0 ; i < 4 ; i++)
        engine->addPlayer("P" + std::to_string(i), std::make_unique<ScriptedMoveProvider>());

    engine->initializeScoreboard(structure, false, false);
    engine->initializeDeck(4);

    return engine;
}
}

TEST_CASE("requestStop() before run() stops the game having played nothing", "[cancellation]")
{
    const auto engine = buildGame();
    Tally tally;
    engine->addObserver(&tally);

    engine->setStatus(GameStatus::InProgress);
    engine->requestStop();
    engine->run();

    REQUIRE(engine->getStatus() == GameStatus::Stopped);
    REQUIRE(tally.gameStopped == 1);
    REQUIRE(tally.gameOver == 0);

    // Not a single round dealt, and the schedule untouched.
    REQUIRE(tally.roundsStarted == 0);
    REQUIRE(tally.roundsScored == 0);
    REQUIRE(engine->getCurrentRoundIndex() == 0);
    REQUIRE(engine->getPhase() == GamePhase::NotStarted);
}

TEST_CASE("requestStop() mid-round lets the trick finish and stops before the next", "[cancellation]")
{
    const auto engine = buildGame();
    Tally tally;
    tally.stopAfterTrick = engine.get();
    tally.stopAfterTrickNumber = 3;

    engine->addObserver(&tally);
    engine->setStatus(GameStatus::InProgress);
    engine->run();

    REQUIRE(engine->getStatus() == GameStatus::Stopped);
    REQUIRE(tally.gameStopped == 1);
    REQUIRE(tally.gameOver == 0);

    // The flag is read at the trick boundary, not mid-turn, so trick 3 is
    // played out in full - and trick 4 never starts.
    REQUIRE(tally.tricksWon == 3);
    REQUIRE(engine->getCurrentTrickNumber() == 3);

    // The round it stopped in is left unscored, and stays the current round.
    REQUIRE(tally.roundsStarted == 1);
    REQUIRE(tally.roundsScored == 0);
    REQUIRE(engine->getCurrentRoundIndex() == 0);

    // GameStatus::Stopped deliberately has no phase of its own: getStatus()
    // says the game is over, getPhase() says where in the round it stopped.
    REQUIRE(engine->getPhase() == GamePhase::Playing);

    // Nobody's turn any more.
    REQUIRE_FALSE(engine->getActiveSeat().has_value());
}

TEST_CASE("A stopped game cannot be run again", "[cancellation]")
{
    const auto engine = buildGame();
    Tally tally;
    engine->addObserver(&tally);

    engine->setStatus(GameStatus::InProgress);
    engine->requestStop();
    engine->run();

    REQUIRE(engine->getStatus() == GameStatus::Stopped);

    // A stopped engine is not resumable, and neither entry point pretends
    // otherwise by quietly returning having done nothing.
    REQUIRE_THROWS_AS(engine->run(), std::logic_error);
    REQUIRE_THROWS_AS(engine->playRound(), std::logic_error);

    // And it is reported once, however many times it is asked.
    REQUIRE(tally.gameStopped == 1);
}

TEST_CASE("run() and playRound() need a game that has been started", "[cancellation]")
{
    const auto engine = buildGame();

    // Never started: the whole hazard is a run() that silently returns having
    // played nothing, which is exactly what a client that forgets to start the
    // game would get.
    REQUIRE_THROWS_AS(engine->run(), std::logic_error);
    REQUIRE_THROWS_AS(engine->playRound(), std::logic_error);

    engine->setStatus(GameStatus::InProgress);
    REQUIRE_NOTHROW(engine->playRound());
}

TEST_CASE("A finished game cannot be run again", "[cancellation]")
{
    // S_181 with 4 players is 24 rounds; play it out and then ask for more.
    const auto engine = buildGame(GameStructure::S_181);
    Tally tally;
    engine->addObserver(&tally);

    engine->setStatus(GameStatus::InProgress);
    engine->run();

    REQUIRE(engine->getStatus() == GameStatus::Finished);
    REQUIRE(engine->getPhase() == GamePhase::GameOver);
    REQUIRE(tally.gameOver == 1);
    REQUIRE(tally.gameStopped == 0);
    REQUIRE(tally.roundsScored == engine->getRoundCount());

    REQUIRE_THROWS_AS(engine->run(), std::logic_error);
    REQUIRE_THROWS_AS(engine->playRound(), std::logic_error);
}
