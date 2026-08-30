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

    SECTION("and neither ending can be asked for from outside")
    {
        // Both terminal states belong to the engine, and both owe observers a
        // callback: onGameOver() arrives with the last round, onGameStopped()
        // at the trick boundary requestStop() lands on. Writing either here
        // used to be allowed and notified nobody, which left every renderer
        // parked on a game that had silently ended - and unrecoverably so,
        // since a terminal status refuses to go back to InProgress.
        REQUIRE_THROWS_AS(engine.setStatus(GameStatus::Finished), std::logic_error);
        REQUIRE_THROWS_AS(engine.setStatus(GameStatus::Stopped), std::logic_error);

        REQUIRE(engine.getStatus() == GameStatus::InProgress);
        REQUIRE(engine.isInProgress());
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

TEST_CASE("The observer list cannot be changed from inside a callback", "[observer]")
{
    // Dispatch walks `observers` directly. A removal mid-walk shifts the tail
    // down under the iterator, so the next observer is skipped and the last one
    // is called twice - the second time from a slot past the vector's own end.
    // Both are rejected instead, loudly, like every other misuse the engine
    // guards.
    struct SelfMutatingObserver : IGameObserver
    {
        GameEngine* engine = nullptr;

        // What to attempt from inside the callback, and what came back.
        bool removeSelf = false;
        IGameObserver* observerToAdd = nullptr;

        unsigned int gameStarted = 0;
        bool threw = false;

        void onGameStarted(const GameEngine&) override
        {
            gameStarted++;

            try
            {
                if(removeSelf)
                    engine->removeObserver(this);

                if(observerToAdd != nullptr)
                    engine->addObserver(observerToAdd);
            }
            catch(const std::logic_error&)
            {
                threw = true;
            }
        }
    };

    auto enginePtr = buildEngine(3);
    GameEngine& engine = *enginePtr;

    SelfMutatingObserver mutator;
    CountingObserver next;
    CountingObserver last;
    mutator.engine = &engine;

    SECTION("removing from inside a callback throws and leaves the walk intact")
    {
        // Registered first, so the erase shifts the whole tail down under an
        // iterator that has already passed it. Unguarded, that is what skips
        // `next` and dispatches `last` twice - the second time from the slot
        // the erase left behind, past the vector's own size.
        mutator.removeSelf = true;

        engine.addObserver(&mutator);
        engine.addObserver(&next);
        engine.addObserver(&last);
        engine.setStatus(GameStatus::InProgress);

        REQUIRE(mutator.threw);

        // The list the walk started with is the list it finished with: each of
        // the three told exactly once, nobody skipped and nobody told twice.
        REQUIRE(mutator.gameStarted == 1);
        REQUIRE(next.gameStarted == 1);
        REQUIRE(last.gameStarted == 1);

        // And the refusal left the list alone rather than half-mutating it.
        REQUIRE_NOTHROW(engine.removeObserver(&mutator));
    }

    SECTION("adding from inside a callback throws too")
    {
        CountingObserver latecomer;
        mutator.observerToAdd = &latecomer;

        engine.addObserver(&mutator);
        engine.setStatus(GameStatus::InProgress);

        REQUIRE(mutator.threw);

        // A push_back mid-walk can reallocate the vector out from under the
        // iterator, so this is rejected for the same reason - and the observer
        // that did not exist when the event began is not told about it.
        REQUIRE(latecomer.gameStarted == 0);
    }

    SECTION("the ban lifts once the dispatch is over")
    {
        // The flag is lowered on the way out of every dispatch, including one a
        // callback threw through, so a client tearing its observers down after
        // the game is not met with a stale refusal.
        mutator.removeSelf = true;

        engine.addObserver(&mutator);
        engine.setStatus(GameStatus::InProgress);

        REQUIRE(mutator.threw);
        REQUIRE_NOTHROW(engine.removeObserver(&mutator));
        REQUIRE_NOTHROW(engine.addObserver(&next));
    }
}

TEST_CASE("A nested dispatch leaves the outer one guarded", "[observer]")
{
    // An observer is free to call back into the engine from a callback, and
    // playRound() dispatches in its own right - so dispatches nest. The guard
    // is what makes the outer walk safe, and it has to survive the inner one
    // finishing: the flag is restored on the way out, not cleared, or every
    // observer after the nested caller could mutate the list mid-walk.
    struct NestingObserver : IGameObserver
    {
        GameEngine* engine = nullptr;
        bool nested = false;

        void onGameStarted(const GameEngine&) override
        {
            // Registered first, so this runs while the walk still has two
            // observers to go.
            engine->playRound();
            nested = true;
        }
    };

    auto enginePtr = buildEngine(3);
    GameEngine& engine = *enginePtr;

    struct RemovingObserver : IGameObserver
    {
        GameEngine* engine = nullptr;
        bool threw = false;
        unsigned int gameStarted = 0;

        void onGameStarted(const GameEngine&) override
        {
            gameStarted++;

            try
            {
                engine->removeObserver(this);
            }
            catch(const std::logic_error&)
            {
                threw = true;
            }
        }
    };

    NestingObserver nester;
    RemovingObserver remover;
    CountingObserver last;
    nester.engine = &engine;
    remover.engine = &engine;

    engine.addObserver(&nester);
    engine.addObserver(&remover);
    engine.addObserver(&last);

    engine.setStatus(GameStatus::InProgress);

    REQUIRE(nester.nested);

    // The removal is still refused, even though a whole round's worth of
    // dispatches has begun and ended since the outer walk started.
    REQUIRE(remover.threw);

    // Which is the point: an accepted removal here shifts `last` down under an
    // iterator that has already passed `remover`, and the walk then reads the
    // slot past the vector's new size.
    REQUIRE(remover.gameStarted == 1);
    REQUIRE(last.gameStarted == 1);

    // And the ban still lifts once the outer walk is over.
    REQUIRE_NOTHROW(engine.removeObserver(&remover));
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
