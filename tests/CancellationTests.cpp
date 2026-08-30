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

    // Stop on the last trick of the last round instead of on a fixed trick
    // number - the one boundary run() cannot catch, since there is no next
    // round for it to check at.
    bool stopOnFinalTrickOfGame = false;

    void onRoundStarted(const GameEngine&) override { roundsStarted++; }
    void onRoundScored(const GameEngine&) override { roundsScored++; }
    void onGameOver(const GameEngine&) override { gameOver++; }
    void onGameStopped(const GameEngine&) override { gameStopped++; }

    void onTrickWon(const GameEngine& engine, Seat, unsigned int trickNumber) override
    {
        tricksWon++;

        if(stopAfterTrick == nullptr)
            return;

        if(stopOnFinalTrickOfGame)
        {
            if(trickNumber == engine.getCurrentRoundTrickCount()
               && engine.getCurrentRoundIndex() + 1 == engine.getRoundCount())
                stopAfterTrick->requestStop();

            return;
        }

        if(trickNumber == stopAfterTrickNumber)
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

TEST_CASE("A stop during a round's final trick leaves that round unscored", "[cancellation]")
{
    // The first S_818 round is eight tricks, so trick 8 is the last one - the
    // trick with no boundary after it.
    const auto engine = buildGame();
    Tally tally;
    tally.stopAfterTrick = engine.get();
    tally.stopAfterTrickNumber = 8;

    engine->addObserver(&tally);
    engine->setStatus(GameStatus::InProgress);
    engine->run();

    REQUIRE(engine->getStatus() == GameStatus::Stopped);
    REQUIRE(tally.gameStopped == 1);
    REQUIRE(tally.gameOver == 0);

    // The round was played out in full and then abandoned. Without a stop check
    // after the trick loop this fell straight through to scoring and committed
    // the very round the stop was meant to abandon.
    REQUIRE(tally.tricksWon == 8);
    REQUIRE(tally.roundsStarted == 1);
    REQUIRE(tally.roundsScored == 0);
    REQUIRE(engine->getCurrentRoundIndex() == 0);
    REQUIRE(engine->getPhase() == GamePhase::Playing);
}

TEST_CASE("A stop on the last trick of the last round stops rather than finishes", "[cancellation]")
{
    const auto engine = buildGame(GameStructure::S_181);
    Tally tally;
    tally.stopAfterTrick = engine.get();
    tally.stopOnFinalTrickOfGame = true;

    engine->addObserver(&tally);
    engine->setStatus(GameStatus::InProgress);
    engine->run();

    // The stop wins. run()'s own boundary check cannot help here - the schedule
    // has no next round to catch it at - so the game used to end Finished with
    // onGameOver() and never fire onGameStopped() at all, despite requestStop()
    // promising status -> Stopped.
    REQUIRE(engine->getStatus() == GameStatus::Stopped);
    REQUIRE(tally.gameStopped == 1);
    REQUIRE(tally.gameOver == 0);

    // Every round but the last scored; the last one abandoned like any other.
    REQUIRE(tally.roundsScored == engine->getRoundCount() - 1);
    REQUIRE(engine->getCurrentRoundIndex() + 1 == engine->getRoundCount());
    REQUIRE(engine->getPhase() == GamePhase::Playing);
}

TEST_CASE("A stop requested after the last round has been scored is a no-op", "[cancellation]")
{
    // The boundary the flag is read at is the one before a round's work, and
    // after the final round's scoring there is none left. On any earlier round
    // run() catches this stop before the next deal; on the last one the game is
    // already Finished by the time the callback runs, so the stop names no
    // round to abandon and Finished stands.
    struct LateStopper : IGameObserver
    {
        GameEngine* engine = nullptr;
        bool stopOnScored = false;

        unsigned int gameOver = 0;
        unsigned int gameStopped = 0;
        unsigned int roundsScored = 0;

        void onGameOver(const GameEngine&) override { gameOver++; }
        void onGameStopped(const GameEngine&) override { gameStopped++; }

        void onRoundScored(const GameEngine& e) override
        {
            roundsScored++;

            if(stopOnScored && e.getCurrentRoundIndex() + 1 == e.getRoundCount())
                engine->requestStop();
        }

        void onRoundComplete(const GameEngine& e) override
        {
            if(!stopOnScored && e.getStatus() == GameStatus::Finished)
                engine->requestStop();
        }
    };

    LateStopper stopper;

    SECTION("from onRoundScored on the final round")
    {
        stopper.stopOnScored = true;
    }

    SECTION("from onRoundComplete on the final round")
    {
        stopper.stopOnScored = false;
    }

    const auto engine = buildGame(GameStructure::S_181);
    stopper.engine = engine.get();

    engine->addObserver(&stopper);
    engine->setStatus(GameStatus::InProgress);
    engine->run();

    // Finished wins, and run() does not turn round and report a stop it can no
    // longer honour - onGameOver() has already told the observers the game was
    // over on its own terms.
    REQUIRE(engine->getStatus() == GameStatus::Finished);
    REQUIRE(stopper.gameOver == 1);
    REQUIRE(stopper.gameStopped == 0);

    // The whole schedule played, including the round the stop was raised in -
    // which is the same treatment an earlier round gets, since a stop raised
    // after a round is scored never un-scores it.
    REQUIRE(stopper.roundsScored == engine->getRoundCount());
    REQUIRE(engine->getPhase() == GamePhase::GameOver);

    // Terminal is terminal: the raised flag does not let the game be restarted
    // and then stopped, which would fire onGameStopped() after onGameOver().
    REQUIRE_THROWS_AS(engine->setStatus(GameStatus::InProgress), std::logic_error);
    REQUIRE(stopper.gameStopped == 0);
}

TEST_CASE("A terminal game cannot be restarted through setStatus", "[cancellation]")
{
    SECTION("a stopped one")
    {
        const auto engine = buildGame();
        Tally tally;
        engine->addObserver(&tally);

        engine->setStatus(GameStatus::InProgress);
        engine->requestStop();
        engine->run();

        REQUIRE(engine->getStatus() == GameStatus::Stopped);

        // This used to be allowed, and the stop flag is never cleared - so the
        // restarted engine stopped again at once and fired onGameStopped() a
        // second time, against its "Fires once" contract.
        REQUIRE_THROWS_AS(engine->setStatus(GameStatus::InProgress), std::logic_error);

        REQUIRE(engine->getStatus() == GameStatus::Stopped);
        REQUIRE(tally.gameStopped == 1);
    }

    SECTION("a finished one")
    {
        const auto engine = buildGame(GameStructure::S_181);
        Tally tally;
        engine->addObserver(&tally);

        engine->setStatus(GameStatus::InProgress);
        engine->run();

        REQUIRE(engine->getStatus() == GameStatus::Finished);

        // This used to be allowed too, and left the engine InProgress on a
        // schedule still parked at its last round: run() re-dealt that round,
        // overwrote its bets, and threw from inside Round::addTrick - two calls
        // away from the mistake.
        REQUIRE_THROWS_AS(engine->setStatus(GameStatus::InProgress), std::logic_error);

        REQUIRE(engine->getStatus() == GameStatus::Finished);
        REQUIRE(tally.gameOver == 1);
        REQUIRE(tally.roundsScored == engine->getRoundCount());
    }
}
