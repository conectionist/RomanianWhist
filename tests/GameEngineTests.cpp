#include <catch2/catch_test_macros.hpp>

#include "ScriptedMoveProvider.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/Deck.h>
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>
#include <romanian_whist/strategies/FirstCardStrategy.h>

#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

using namespace romanian_whist;
using namespace romanian_whist::test;

namespace
{
std::unique_ptr<IMoveProvider> dummyProvider()
{
    return std::make_unique<AiMoveProvider>(std::make_unique<FirstCardStrategy>());
}

void addPlayers(GameEngine& engine, unsigned int count)
{
    for(unsigned int i = 0 ; i < count ; i++)
        engine.addPlayer("P" + std::to_string(i), dummyProvider());
}

// The bidding rules used to be testable by hand: place a bet, ask what is
// forbidden, place another. placeBet() is the engine's own business now, so the
// only way in is to watch a real game being played - which is the better test
// anyway, since it checks the rule in the situation it is actually used in.
//
// Every seat is scripted so the game is deterministic and its bids are known.
std::unique_ptr<GameEngine> buildScriptedGame(std::vector<ScriptedMoveProvider*>& seats,
                                              unsigned int playerCount,
                                              GameStructure structure = GameStructure::S_181)
{
    auto engine = std::make_unique<GameEngine>(1u);

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

TEST_CASE("GameEngine::getForbiddenBet names the bid that would complete the round", "[game-engine]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildScriptedGame(seats, 3);

    // Recomputed independently, and deliberately not the way getForbiddenBet()
    // does it: that counts how many bets are already engaged, this asks where
    // the seat sits in the bidding order. Two routes to the same answer, so a
    // mistake in either shows up as a disagreement.
    struct Checker : IGameObserver
    {
        unsigned int total = 0;
        unsigned int seen = 0;

        void onRoundStarted(const GameEngine&) override
        {
            total = 0;
            seen = 0;
        }

        void onBetRequested(const GameEngine& engine, Seat seat) override
        {
            const unsigned int trickCount = engine.getCurrentRoundTrickCount();
            const bool isFinalBidder = engine.getBiddingOrder(seat) == engine.getPlayerCount();

            std::optional<unsigned int> expected;

            // The restriction only binds the last bidder, and only while a bid
            // could still bring the total to exactly the trick count.
            if(isFinalBidder && total <= trickCount)
                expected = trickCount - total;

            REQUIRE(engine.getForbiddenBet() == expected);

            // isBetLegal() folds the same rule together with the range check.
            if(expected)
                REQUIRE_FALSE(engine.isBetLegal(*expected));

            REQUIRE_FALSE(engine.isBetLegal(trickCount + 1));

            // Every bidder but the last has a completely free hand.
            if(!isFinalBidder)
            {
                for(unsigned int bet = 0 ; bet <= trickCount ; bet++)
                    REQUIRE(engine.isBetLegal(bet));
            }

            seen++;
        }

        void onBetPlaced(const GameEngine&, Seat, unsigned int bet) override
        {
            total += bet;
        }

        void onBettingComplete(const GameEngine& engine) override
        {
            REQUIRE(seen == engine.getPlayerCount());

            // Everyone has bid, so there is no final bidder left to restrict.
            REQUIRE_FALSE(engine.getForbiddenBet().has_value());
        }
    };

    Checker checker;
    engine->addObserver(&checker);
    engine->run();

    REQUIRE(engine->getStatus() == GameStatus::Finished);
}

TEST_CASE("GameEngine::getForbiddenBet is empty once the bids already exceed the trick count", "[game-engine]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildScriptedGame(seats, 3);

    // Round 3 of a 3-player S_181 schedule is the first 2-trick round: the
    // three 1-trick rounds come first. Overbid it with the first two seats so
    // no bid the third could make can bring the total back down to 2.
    seats[0]->bids = { 0, 0, 0, 2 };
    seats[1]->bids = { 0, 0, 0, 2 };

    struct Checker : IGameObserver
    {
        bool checkedOverbidRound = false;

        void onBetRequested(const GameEngine& engine, Seat seat) override
        {
            if(engine.getCurrentRoundIndex() != 3)
                return;

            if(engine.getBiddingOrder(seat) != engine.getPlayerCount())
                return;

            REQUIRE(engine.getCurrentRoundTrickCount() == 2);

            // No value at all is barred: the round can no longer add up
            // exactly, so there is nothing left to prevent.
            REQUIRE_FALSE(engine.getForbiddenBet().has_value());

            // Which means every bid in range is legal, including the one that
            // would have been barred had the total still been reachable.
            REQUIRE(engine.isBetLegal(0));
            REQUIRE(engine.isBetLegal(2));

            checkedOverbidRound = true;
        }
    };

    Checker checker;
    engine->addObserver(&checker);
    engine->run();

    REQUIRE(checker.checkedOverbidRound);
}

TEST_CASE("Round-scoped accessors throw before the game is set up", "[game-engine]")
{
    GameEngine engine(1u);
    addPlayers(engine, 3);

    // Scoreboard::getCurrentRound() is rounds[currentRound] on a vector that is
    // empty until initializeScoreboard() runs, so every one of these would
    // otherwise read arbitrary memory rather than complain. A client with a
    // setup screen - a Qt window drawing a scoreboard widget before the wizard
    // finishes, a backend serving a game that was created but never started -
    // walks straight into it.
    REQUIRE_THROWS_AS(engine.getCurrentRound(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getCurrentRoundIndex(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getCurrentRoundTrickCount(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getCurrentRoundType(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getCurrentTrumpCard(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getRoundLeaderSeat(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getTrickLeaderSeat(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getBiddingOrder(Seat{0}), std::logic_error);
    REQUIRE_THROWS_AS(engine.getForbiddenBet(), std::logic_error);
    REQUIRE_THROWS_AS(engine.isBetLegal(0), std::logic_error);
    REQUIRE_THROWS_AS(engine.getCurrentTrick(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getCurrentTrickLeader(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getCurrentTrickNumber(), std::logic_error);
    REQUIRE_THROWS_AS(engine.getBet(Seat{0}), std::logic_error);
    REQUIRE_THROWS_AS(engine.getTricksWon(Seat{0}), std::logic_error);

    // These four answer at any time, and are how a client asks whether the rest
    // are safe to call yet.
    REQUIRE(engine.getPhase() == GamePhase::NotStarted);
    REQUIRE(engine.getStatus() == GameStatus::NotStarted);
    REQUIRE_FALSE(engine.isInProgress());
    REQUIRE(engine.getPlayerCount() == 3);
    REQUIRE_FALSE(engine.getActiveSeat().has_value());

    engine.initializeScoreboard(GameStructure::S_181, false, false);

    REQUIRE_NOTHROW(engine.getCurrentRound());
    REQUIRE_NOTHROW(engine.getCurrentTrick());
    REQUIRE(engine.getCurrentTrickNumber() == 0u);
}

TEST_CASE("GameEngine::getBiddingOrder counts round from the round leader", "[game-engine]")
{
    std::vector<ScriptedMoveProvider*> seats;
    const auto engine = buildScriptedGame(seats, 4);

    struct Checker : IGameObserver
    {
        unsigned int roundsChecked = 0;

        void onRoundStarted(const GameEngine& engine) override
        {
            const unsigned int playerCount = engine.getPlayerCount();

            Seat seat = engine.getRoundLeaderSeat();

            for(unsigned int position = 1 ; position <= playerCount ; position++)
            {
                REQUIRE(engine.getBiddingOrder(seat) == position);
                seat = engine.getNextSeat(seat);
            }

            // Back where it started, having named every seat exactly once.
            REQUIRE(seat == engine.getRoundLeaderSeat());

            roundsChecked++;
        }
    };

    Checker checker;
    engine->addObserver(&checker);
    engine->run();

    // Checked in every round, because the round leader advances one seat per
    // round: a getBiddingOrder() measured from a fixed seat, or from the trick
    // leader, would still pass on round 0 alone.
    REQUIRE(checker.roundsChecked == engine->getRoundCount());
    REQUIRE_THROWS_AS(engine->getBiddingOrder(Seat{4}), std::out_of_range);
}

TEST_CASE("GameEngine::addPlayer rejects duplicate names", "[game-engine]")
{
    GameEngine engine;
    engine.addPlayer("A", dummyProvider());

    REQUIRE_THROWS_AS(engine.addPlayer("A", dummyProvider()), std::invalid_argument);
    REQUIRE(engine.getPlayerCount() == 1);
}

TEST_CASE("GameEngine::initializeDeck rejects a second call", "[game-engine]")
{
    // A second call used to be made "safe" by clearing and rebuilding the
    // deck, but that dangles every Card* already handed out by a deal (the
    // round's trump, any recorded tricks, every player's hand) - rejecting
    // it outright is the only thing that is actually safe.
    GameEngine engine;
    addPlayers(engine, 4);
    engine.initializeDeck(4);

    REQUIRE_THROWS_AS(engine.initializeDeck(4), std::logic_error);
    REQUIRE(engine.getDeck().size() == 32);
}

TEST_CASE("GameEngine::addPlayer rejects a player added after initializeDeck", "[game-engine]")
{
    // initializeDeck()'s playerCount-matches-players.size() guard only
    // checks at the moment it runs; a seat added afterwards would silently
    // invalidate it again.
    GameEngine engine;
    addPlayers(engine, 4);
    engine.initializeDeck(4);

    REQUIRE_THROWS_AS(engine.addPlayer("extra", dummyProvider()), std::logic_error);
    REQUIRE(engine.getPlayerCount() == 4);
}

TEST_CASE("GameEngine::addPlayer rejects a player added after initializeScoreboard", "[game-engine]")
{
    // The schedule's length and its opener rotation are both laid out for
    // the player count at the moment initializeScoreboard() runs; a seat
    // added afterwards used to pass every check and leave a 4-player game
    // running a 3-player, 21-round schedule.
    GameEngine engine;
    addPlayers(engine, 3);
    engine.initializeScoreboard(GameStructure::S_181, false, false);
    const unsigned int roundCount = engine.getRoundCount();

    REQUIRE_THROWS_AS(engine.addPlayer("extra", dummyProvider()), std::logic_error);
    REQUIRE(engine.getPlayerCount() == 3);
    REQUIRE(engine.getRoundCount() == roundCount);
}

TEST_CASE("GameEngine::initializeScoreboard rejects a second call", "[game-engine]")
{
    // Scoreboard::initialize() appends without clearing, so a second call
    // used to double the schedule - 21 rounds became 42, with the opener
    // rotation restarting halfway through.
    GameEngine engine;
    addPlayers(engine, 3);
    engine.initializeScoreboard(GameStructure::S_181, false, false);
    const unsigned int roundCount = engine.getRoundCount();
    REQUIRE(roundCount == 21);

    REQUIRE_THROWS_AS(engine.initializeScoreboard(GameStructure::S_181, false, false),
                      std::logic_error);
    REQUIRE(engine.getRoundCount() == roundCount);
}

TEST_CASE("playRound() requires the deck and the scoreboard", "[game-engine]")
{
    // dealCards() is the engine's own business now, so these guards are reached
    // through the loop rather than directly - which is the only path a client
    // can actually take.
    SECTION("without initializeDeck it throws instead of dealing out of an empty deck")
    {
        // Every player would otherwise be handed a Card* into an empty Deck,
        // with the crash deferred to the first dereference.
        GameEngine engine;
        addPlayers(engine, 3);
        engine.initializeScoreboard(GameStructure::S_181, false, false);
        engine.setStatus(GameStatus::InProgress);

        REQUIRE_THROWS_AS(engine.playRound(), std::logic_error);
    }

    SECTION("without initializeScoreboard it throws instead of reading a missing round")
    {
        GameEngine engine;
        addPlayers(engine, 3);
        engine.initializeDeck(3);
        engine.setStatus(GameStatus::InProgress);

        REQUIRE_THROWS_AS(engine.playRound(), std::logic_error);
    }

    SECTION("with both in place it plays")
    {
        GameEngine engine;
        addPlayers(engine, 3);
        engine.initializeScoreboard(GameStructure::S_181, false, false);
        engine.initializeDeck(3);
        engine.setStatus(GameStatus::InProgress);

        REQUIRE_NOTHROW(engine.playRound());
    }
}

TEST_CASE("GameEngine::initializeDeck rejects a playerCount that doesn't match the players added", "[game-engine]")
{
    // dealCards() indexes the deck by players.size(), not by initializeDeck's
    // argument - a mismatch here used to build a deck sized for the wrong
    // player count and read past the end of it once dealt.
    GameEngine engine;
    addPlayers(engine, 6);

    REQUIRE_THROWS_AS(engine.initializeDeck(4), std::invalid_argument);
}

TEST_CASE("GameEngine::initializeDeck rejects impossible player counts", "[game-engine]")
{
    SECTION("1 player throws")
    {
        GameEngine engine;
        REQUIRE_THROWS_AS(engine.initializeDeck(1), std::invalid_argument);
    }

    SECTION("7 players throws")
    {
        GameEngine engine;
        REQUIRE_THROWS_AS(engine.initializeDeck(7), std::invalid_argument);
    }

    SECTION("2 through 6 players do not throw")
    {
        for(unsigned int n = 2 ; n <= 6 ; n++)
        {
            GameEngine engine;
            addPlayers(engine, n);
            REQUIRE_NOTHROW(engine.initializeDeck(n));
        }
    }
}

TEST_CASE("GameEngine deck composition", "[game-engine]")
{
    struct Expectation { unsigned int playerCount; std::size_t size; Rank lowest; };

    const std::vector<Expectation> expectations{
        { 2, 16, Rank::Jack },
        { 3, 24, Rank::Nine },
        { 4, 32, Rank::Seven },
        { 5, 40, Rank::Five },
        { 6, 48, Rank::Three },
    };

    for(const auto& expectation : expectations)
    {
        GameEngine engine;
        addPlayers(engine, expectation.playerCount);
        engine.initializeDeck(expectation.playerCount);

        const Deck& deck = engine.getDeck();
        REQUIRE(deck.size() == expectation.size);

        Rank lowest = Rank::Ace;
        bool sawTwo = false;
        for(const Card& card : deck)
        {
            if(static_cast<int>(card.rank) < static_cast<int>(lowest))
                lowest = card.rank;
            if(card.rank == Rank::Two)
                sawTwo = true;
        }

        REQUIRE(lowest == expectation.lowest);
        REQUIRE_FALSE(sawTwo);
    }
}
