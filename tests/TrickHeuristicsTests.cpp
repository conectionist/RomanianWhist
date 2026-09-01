#include <catch2/catch_test_macros.hpp>

#include "CardStringMaker.h"

#include <romanian_whist/PlayContext.h>
#include <romanian_whist/strategies/TrickHeuristics.h>

#include <optional>
#include <vector>

using namespace romanian_whist;

// The ordering these run on is WEAK, not total: two plain cards of the same
// rank in different suits compare equivalent, and so do two such trumps. Which
// of a run of equivalents comes back is therefore settled by position, not by
// the comparator - mostDangerous() and leastDangerous() are max_element and
// min_element, and both return the FIRST of a run.
//
// That is load-bearing. Every strategy plays whatever these return, so the
// order of the list handed to them decides which card is played, without
// changing a single rule. getLegalCards(), safeCards() and winningCards() all
// push in input order to keep it stable.
//
// Nothing pinned that before this file. The Phase 4 port was reviewed against
// exactly this hazard and the invariant was recorded only in comments - so a
// later edit that reordered any of those three, or reached for an algorithm
// with a different tie rule, would have changed play and shown up as six moved
// golden scores rather than as one located failure.

namespace
{
const Card SpadesQueen(Rank::Queen, Suit::Spades);
const Card HeartsQueen(Rank::Queen, Suit::Hearts);
const Card ClubsQueen(Rank::Queen, Suit::Clubs);
const Card HeartsAce(Rank::Ace, Suit::Hearts);
const Card HeartsTwo(Rank::Two, Suit::Hearts);
const Card DiamondsThree(Rank::Three, Suit::Diamonds);
}

TEST_CASE("Equally dangerous cards are separated by position, not by the comparator", "[heuristics]")
{
    // Same rank, different plain suits: neither is more dangerous than the
    // other, so the comparator cannot choose between them.
    REQUIRE_FALSE(heuristics::isMoreDangerous(SpadesQueen, HeartsQueen, std::nullopt));
    REQUIRE_FALSE(heuristics::isMoreDangerous(HeartsQueen, SpadesQueen, std::nullopt));

    SECTION("both extremes return the first of a run of equivalents")
    {
        const std::vector<Card> cards{ SpadesQueen, HeartsQueen, ClubsQueen };

        REQUIRE(heuristics::mostDangerous(cards, std::nullopt) == SpadesQueen);
        REQUIRE(heuristics::leastDangerous(cards, std::nullopt) == SpadesQueen);
    }

    SECTION("so reversing the input reverses the answer, and that is the whole hazard")
    {
        const std::vector<Card> reversed{ ClubsQueen, HeartsQueen, SpadesQueen };

        REQUIRE(heuristics::mostDangerous(reversed, std::nullopt) == ClubsQueen);
        REQUIRE(heuristics::leastDangerous(reversed, std::nullopt) == ClubsQueen);
    }

    SECTION("a genuine difference still decides, whatever the order")
    {
        const std::vector<Card> mixed{ HeartsTwo, HeartsAce, SpadesQueen };

        REQUIRE(heuristics::mostDangerous(mixed, std::nullopt) == HeartsAce);
        REQUIRE(heuristics::leastDangerous(mixed, std::nullopt) == HeartsTwo);
    }

    SECTION("an empty list has no answer to give")
    {
        REQUIRE(heuristics::mostDangerous({}, std::nullopt) == std::nullopt);
        REQUIRE(heuristics::leastDangerous({}, std::nullopt) == std::nullopt);
    }
}

TEST_CASE("Trump outranks every plain card, and ties among trumps go by position too", "[heuristics]")
{
    const Card trump(Rank::Two, Suit::Clubs);

    // A trump takes tricks its rank alone would never justify.
    REQUIRE(heuristics::isMoreDangerous(ClubsQueen, HeartsAce, trump));
    REQUIRE_FALSE(heuristics::isMoreDangerous(HeartsAce, ClubsQueen, trump));

    // With clubs trump, the club is the dangerous one however low it ranks.
    const std::vector<Card> cards{ HeartsAce, ClubsQueen };

    REQUIRE(heuristics::mostDangerous(cards, trump) == ClubsQueen);
    REQUIRE(heuristics::leastDangerous(cards, trump) == HeartsAce);
}

TEST_CASE("chooseDuckingCard sheds the first of equally safe discards", "[heuristics]")
{
    // Following hearts with no heart and no trump: both queens are discards,
    // neither can take the trick, and neither is more dangerous than the other.
    // Which one goes is decided by where safeCards() left them - so this is the
    // assertion that pins safeCards() preserving input order, not just the
    // extremes above.
    const std::vector<Card> hand{ SpadesQueen, ClubsQueen };
    const std::vector<Card> played{ HeartsAce };

    PlayContext context{ hand, played, std::nullopt, Suit::Hearts, 0, 0 };

    REQUIRE(heuristics::chooseDuckingCard(context, hand) == SpadesQueen);

    const std::vector<Card> reversed{ ClubsQueen, SpadesQueen };
    PlayContext reversedContext{ reversed, played, std::nullopt, Suit::Hearts, 0, 0 };

    REQUIRE(heuristics::chooseDuckingCard(reversedContext, reversed) == ClubsQueen);
}

TEST_CASE("chooseWinningCard leads the first of equally dangerous cards", "[heuristics]")
{
    // Leading, so it plays its most dangerous card - and with two equivalent
    // ones that is a positional choice.
    //
    // Note the mirror case cannot arise: a tie inside winningCards() is
    // impossible, because taking the trick means following the lead suit or
    // trumping, and two cards of one suit never share a rank. Ties only ever
    // reach these functions from the whole hand or from a set of discards.
    const std::vector<Card> hand{ SpadesQueen, HeartsQueen, DiamondsThree };
    const std::vector<Card> nothingPlayed;

    PlayContext context{ hand, nothingPlayed, std::nullopt, std::nullopt, 1, 0 };

    REQUIRE(heuristics::chooseWinningCard(context, hand) == SpadesQueen);
}
