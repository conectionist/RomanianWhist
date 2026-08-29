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

    auto first = engine.getFirstPlayerOfTheRound();
    auto second = engine.getNextPlayer(first);
    auto third = engine.getNextPlayer(second);

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

    SECTION("setResult before any bet does not corrupt getForbiddenBet")
    {
        // Round::setResult() writes into the same map entry setBet() does;
        // hasBet() has to tell a real bet apart from a result seeded first
        // (Round.h), or every seat looks like it has already bid and
        // getForbiddenBet() goes blind for the rest of the round.
        engine.setResult(first, 0);
        engine.setResult(second, 0);
        engine.setResult(third, 0);

        REQUIRE_FALSE(engine.getForbiddenBet().has_value());

        engine.placeBet(first, 0);
        engine.placeBet(second, 1);

        const auto forbidden = engine.getForbiddenBet();
        REQUIRE(forbidden.has_value());
        REQUIRE(*forbidden == 1u);
        REQUIRE_FALSE(engine.isBetLegal(1));
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

    auto first = engine.getFirstPlayerOfTheRound();
    auto second = engine.getNextPlayer(first);

    engine.placeBet(first, 0);
    engine.placeBet(second, 1);

    REQUIRE(engine.getForbiddenBet() == 1u);

    REQUIRE_FALSE(engine.isBetLegal(3));   // exceeds trick count
    REQUIRE_FALSE(engine.isBetLegal(1));   // the forbidden value
    REQUIRE(engine.isBetLegal(0));
    REQUIRE(engine.isBetLegal(2));
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
