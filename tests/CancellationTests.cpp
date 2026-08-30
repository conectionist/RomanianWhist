#include <catch2/catch_test_macros.hpp>

#include "ScriptedMoveProvider.h"

#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>

#include <memory>
#include <optional>
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
    unsigned int betsRequested = 0;
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
    void onBetRequested(const GameEngine&, Seat) override { betsRequested++; }
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

TEST_CASE("playRound() reads the stop flag before it deals", "[cancellation]")
{
    // playRound() is a documented entry point, not just run()'s helper, so it
    // owes a stop the same answer run() gives. The check used to live only in
    // run()'s loop, which left a client driving rounds itself dealing one more
    // round after requestStop() - and, with a human provider, asking every
    // player to bid on a hand thrown away a moment later.
    const auto engine = buildGame();
    Tally tally;
    engine->addObserver(&tally);

    engine->setStatus(GameStatus::InProgress);
    engine->requestStop();
    engine->playRound();

    REQUIRE(engine->getStatus() == GameStatus::Stopped);
    REQUIRE(tally.gameStopped == 1);
    REQUIRE(tally.gameOver == 0);

    // Nothing dealt, nobody asked to bid, the schedule untouched - the same
    // state the run() version of this test pins.
    REQUIRE(tally.roundsStarted == 0);
    REQUIRE(tally.betsRequested == 0);
    REQUIRE(tally.roundsScored == 0);
    REQUIRE(engine->getCurrentRoundIndex() == 0);
    REQUIRE(engine->getPhase() == GamePhase::NotStarted);
}

TEST_CASE("The trick number moves with the round index", "[cancellation]")
{
    // getCurrentTrickNumber() counts tricks within the round getCurrentRoundIndex()
    // names, so the two have to advance together. completeCurrentRound() used to
    // move the index alone and leave the finished round's last trick number
    // standing - visible at onRoundComplete(), and at onGameStopped() for a stop
    // honoured between rounds. In an S_818 game that reads as "trick 8" over a
    // round with one trick in it.
    struct Watcher : IGameObserver
    {
        GameEngine* engine = nullptr;
        bool stopOnFirstRoundScored = false;

        std::optional<unsigned int> trickNumberAtRoundComplete;
        std::optional<unsigned int> trickNumberAtStop;
        std::optional<unsigned int> trickCountAtStop;

        void onRoundScored(const GameEngine& e) override
        {
            if(stopOnFirstRoundScored && e.getCurrentRoundIndex() == 0)
                engine->requestStop();
        }

        void onRoundComplete(const GameEngine& e) override
        {
            if(!trickNumberAtRoundComplete.has_value())
                trickNumberAtRoundComplete = e.getCurrentTrickNumber();
        }

        void onGameStopped(const GameEngine& e) override
        {
            trickNumberAtStop = e.getCurrentTrickNumber();
            trickCountAtStop = e.getCurrentRoundTrickCount();
        }
    };

    Watcher watcher;

    SECTION("at onRoundComplete")
    {
        watcher.stopOnFirstRoundScored = false;
    }

    SECTION("and at onGameStopped for a stop honoured between rounds")
    {
        watcher.stopOnFirstRoundScored = true;
    }

    const auto engine = buildGame();
    watcher.engine = engine.get();

    engine->addObserver(&watcher);
    engine->setStatus(GameStatus::InProgress);
    engine->run();

    // The first round's eight tricks are all played; the index then moves to
    // round 1, and the trick number goes back to 0 with it - there is no
    // in-flight trick at a round boundary for it to name.
    REQUIRE(watcher.trickNumberAtRoundComplete.has_value());
    REQUIRE(*watcher.trickNumberAtRoundComplete == 0);

    if(watcher.stopOnFirstRoundScored)
    {
        REQUIRE(engine->getStatus() == GameStatus::Stopped);
        REQUIRE(engine->getCurrentRoundIndex() == 1);

        REQUIRE(watcher.trickNumberAtStop.has_value());
        REQUIRE(*watcher.trickNumberAtStop == 0);

        // The number the stop reports has to be one the round it names could
        // actually reach. Round 1 of this 4-player S_818 game has eight tricks,
        // like round 0, so this is the weaker of the two checks - the == 0
        // above is what actually catches a stale trick number here. Kept
        // because it is the invariant that holds for every round, including the
        // 1-trick ones a stop can land between further down the schedule.
        REQUIRE(*watcher.trickNumberAtStop <= *watcher.trickCountAtStop);
    }
}

TEST_CASE("A stop raised during bidding is honoured before the next seat bids",
          "[cancellation]")
{
    // Bidding is a boundary in its own right. Without one, a stop raised in
    // onRoundStarted() or in an early onBetPlaced() still walked the rest of
    // the table collecting bids for a hand it was about to throw away - with a
    // human provider, three prompts for a round nobody will ever play.
    struct Bidder : IGameObserver
    {
        GameEngine* engine = nullptr;

        // Which onBetPlaced to stop on, 1-based; 0 stops from onRoundStarted,
        // before anyone has been asked at all.
        unsigned int stopAfterBet = 0;

        unsigned int betsRequested = 0;
        unsigned int betsPlaced = 0;
        unsigned int bettingComplete = 0;
        unsigned int tricksStarted = 0;
        unsigned int roundsScored = 0;
        unsigned int gameOver = 0;
        unsigned int gameStopped = 0;

        void onRoundStarted(const GameEngine&) override
        {
            if(stopAfterBet == 0)
                engine->requestStop();
        }

        void onBetRequested(const GameEngine&, Seat) override { betsRequested++; }
        void onBettingComplete(const GameEngine&) override { bettingComplete++; }
        void onTrickStarted(const GameEngine&, unsigned int, Seat) override { tricksStarted++; }
        void onRoundScored(const GameEngine&) override { roundsScored++; }
        void onGameOver(const GameEngine&) override { gameOver++; }
        void onGameStopped(const GameEngine&) override { gameStopped++; }

        void onBetPlaced(const GameEngine&, Seat, unsigned int) override
        {
            betsPlaced++;

            if(betsPlaced == stopAfterBet)
                engine->requestStop();
        }
    };

    Bidder bidder;
    unsigned int expectedBetsRequested = 0;

    SECTION("raised in onRoundStarted, before anyone is asked")
    {
        bidder.stopAfterBet = 0;
        expectedBetsRequested = 0;
    }

    SECTION("raised in the first onBetPlaced, so three seats are never asked")
    {
        bidder.stopAfterBet = 1;
        expectedBetsRequested = 1;
    }

    SECTION("raised in the last onBetPlaced, with the table already round")
    {
        // No seat is spared here - the point is the phase and the unscored
        // round, and that the trick loop never starts on bets nobody will use.
        bidder.stopAfterBet = 4;
        expectedBetsRequested = 4;
    }

    const auto engine = buildGame();
    bidder.engine = engine.get();

    engine->addObserver(&bidder);
    engine->setStatus(GameStatus::InProgress);
    engine->run();

    REQUIRE(engine->getStatus() == GameStatus::Stopped);
    REQUIRE(bidder.gameStopped == 1);
    REQUIRE(bidder.gameOver == 0);

    // The seats after the stop are never prompted, which is the whole point.
    REQUIRE(bidder.betsRequested == expectedBetsRequested);

    // Bidding never completed, so no trick was ever started on it, and the
    // round it stopped in is left unscored and still current.
    REQUIRE(bidder.bettingComplete == 0);
    REQUIRE(bidder.tricksStarted == 0);
    REQUIRE(bidder.roundsScored == 0);
    REQUIRE(engine->getCurrentRoundIndex() == 0);
    REQUIRE(engine->getCurrentTrickNumber() == 0);

    // Betting, not Playing: the phase is read before runBidding() hands over,
    // so a stop honoured anywhere in bidding says where it actually landed.
    REQUIRE(engine->getPhase() == GamePhase::Betting);

    // Nobody's turn any more, including when the stop landed between the
    // provider being asked and the next seat being reached.
    REQUIRE_FALSE(engine->getActiveSeat().has_value());
}
