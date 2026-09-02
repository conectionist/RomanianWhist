#include <catch2/catch_test_macros.hpp>

#include "CardStringMaker.h"

#include <romanian_whist/BetContext.h>
#include <romanian_whist/RoundType.h>
#include <romanian_whist/strategies/LowRiskStrategy.h>

#include <optional>
#include <vector>

using namespace romanian_whist;

// The blind-bidding rule was pinned only by one row of the golden game, which
// covers Forehead alone: deleting the Hidden half of the condition in
// getBestBet() left the whole suite green. A golden also cannot say WHY a
// number moved, so the round types are asserted here, directly on the strategy.

namespace
{
const Card HeartsAce(Rank::Ace, Suit::Hearts);
const Card SpadesAce(Rank::Ace, Suit::Spades);
const Card ClubsTwo(Rank::Two, Suit::Clubs);
}

TEST_CASE("LowRiskStrategy bids blind in the round types that hide the hand", "[strategy]")
{
    LowRiskStrategy strategy;

    // Two aces and no trump: a hand countLikelyWinners() reads as two tricks,
    // so a bid of 0 can only come from the round type, never from the cards.
    const std::vector<Card> hand{HeartsAce, SpadesAce, ClubsTwo};

    SECTION("Normal reads the hand")
    {
        BetContext context{hand};
        context.roundType = RoundType::Normal;

        REQUIRE(strategy.getBestBet(context) == 2);
    }

    SECTION("Forehead bids 0")
    {
        BetContext context{hand};
        context.roundType = RoundType::Forehead;

        REQUIRE(strategy.getBestBet(context) == 0);
    }

    SECTION("Hidden bids 0")
    {
        BetContext context{hand};
        context.roundType = RoundType::Hidden;

        REQUIRE(strategy.getBestBet(context) == 0);
    }
}

TEST_CASE("LowRiskStrategy steps a barred blind bid up, not down", "[strategy]")
{
    LowRiskStrategy strategy;

    // Forehead and Hidden rounds are always one trick, so the blind bidder that
    // is barred from 0 has exactly one other bid available. Stepping DOWN from
    // zero is what the general rule would do, and it does not exist.
    const std::vector<Card> hand{ClubsTwo};

    SECTION("Forehead")
    {
        BetContext context{hand};
        context.roundType = RoundType::Forehead;
        context.forbiddenBet = 0u;

        REQUIRE(strategy.getBestBet(context) == 1);
    }

    SECTION("Hidden")
    {
        BetContext context{hand};
        context.roundType = RoundType::Hidden;
        context.forbiddenBet = 0u;

        REQUIRE(strategy.getBestBet(context) == 1);
    }
}
