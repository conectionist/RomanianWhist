#include <catch2/catch_test_macros.hpp>

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/Deck.h>
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/strategies/FirstCardStrategy.h>

#include <stdexcept>

using namespace romanian_whist;

namespace
{
std::unique_ptr<IMoveProvider> dummyProvider()
{
    return std::make_unique<AiMoveProvider>(std::make_unique<FirstCardStrategy>());
}

// Skips `rounds` rounds without playing them, landing the current-round
// pointer somewhere with a chosen trick count - enough for tests that only
// exercise betting mechanics, which don't need a dealt hand.
void skipRounds(GameEngine& engine, unsigned int rounds)
{
    for(unsigned int i = 0 ; i < rounds ; i++)
        engine.completeCurrentRound();
}

void addPlayers(GameEngine& engine, unsigned int count)
{
    for(unsigned int i = 0 ; i < count ; i++)
        engine.addPlayer("P" + std::to_string(i), dummyProvider());
}
}

TEST_CASE("GameEngine::getForbiddenBet", "[game-engine]")
{
    GameEngine engine(1u);
    engine.addPlayer("A", dummyProvider());
    engine.addPlayer("B", dummyProvider());
    engine.addPlayer("C", dummyProvider());
    engine.initializeScoreboard(GameStructure::S_181, false, false);

    // 3 players: schedule is 1,1,1, 2,3,4,5,6,7, ... - skip the three 1-trick
    // rounds to land on the first 2-trick round.
    skipRounds(engine, 3);
    REQUIRE(engine.getCurrentRoundTrickCount() == 2);

    const Seat first = engine.getRoundLeaderSeat();
    const Seat second = engine.getNextSeat(first);

    SECTION("empty before anyone has bid")
    {
        REQUIRE_FALSE(engine.getForbiddenBet().has_value());
    }

    SECTION("empty until only the final bidder is left")
    {
        engine.placeBet(first, 0);
        REQUIRE_FALSE(engine.getForbiddenBet().has_value());
    }

    SECTION("names the value that would complete the round exactly")
    {
        engine.placeBet(first, 0);
        engine.placeBet(second, 1);

        const auto forbidden = engine.getForbiddenBet();
        REQUIRE(forbidden.has_value());
        REQUIRE(*forbidden == 1u);
    }

    SECTION("empty for the final bidder once bids already exceed the trick count")
    {
        engine.placeBet(first, 2);
        engine.placeBet(second, 2);

        REQUIRE_FALSE(engine.getForbiddenBet().has_value());
    }
}

TEST_CASE("GameEngine::isBetLegal", "[game-engine]")
{
    GameEngine engine(1u);
    engine.addPlayer("A", dummyProvider());
    engine.addPlayer("B", dummyProvider());
    engine.addPlayer("C", dummyProvider());
    engine.initializeScoreboard(GameStructure::S_181, false, false);
    skipRounds(engine, 3);
    REQUIRE(engine.getCurrentRoundTrickCount() == 2);

    const Seat first = engine.getRoundLeaderSeat();
    const Seat second = engine.getNextSeat(first);

    engine.placeBet(first, 0);
    engine.placeBet(second, 1);

    REQUIRE(engine.getForbiddenBet() == 1u);

    REQUIRE_FALSE(engine.isBetLegal(3));   // exceeds trick count
    REQUIRE_FALSE(engine.isBetLegal(1));   // the forbidden value
    REQUIRE(engine.isBetLegal(0));
    REQUIRE(engine.isBetLegal(2));
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

TEST_CASE("GameEngine::dealCards requires the deck and the scoreboard", "[game-engine]")
{
    SECTION("without initializeDeck it throws instead of dealing out of an empty deck")
    {
        // Every player would otherwise be handed a Card* into an empty
        // Deck, with the crash deferred to the first dereference.
        GameEngine engine;
        addPlayers(engine, 3);
        engine.initializeScoreboard(GameStructure::S_181, false, false);

        REQUIRE_THROWS_AS(engine.dealCards(), std::logic_error);
    }

    SECTION("without initializeScoreboard it throws instead of reading a missing round")
    {
        GameEngine engine;
        addPlayers(engine, 3);
        engine.initializeDeck(3);

        REQUIRE_THROWS_AS(engine.dealCards(), std::logic_error);
    }

    SECTION("with both in place it deals")
    {
        GameEngine engine;
        addPlayers(engine, 3);
        engine.initializeScoreboard(GameStructure::S_181, false, false);
        engine.initializeDeck(3);
        engine.shuffleDeck();

        REQUIRE_NOTHROW(engine.dealCards());
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
