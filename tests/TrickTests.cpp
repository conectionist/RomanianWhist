#include <catch2/catch_test_macros.hpp>

#include <romanian_whist/Round.h>
#include <romanian_whist/Trick.h>

#include <stdexcept>

using namespace romanian_whist;

TEST_CASE("Trick records who played each card, in play order", "[trick]")
{
    Card heartsTwo(Rank::Two, Suit::Hearts);
    Card heartsKing(Rank::King, Suit::Hearts);
    Card spadesAce(Rank::Ace, Suit::Spades);

    Trick trick;

    // Not in seat order: a trick is led by whoever won the last one, so the
    // seats arrive rotated and the order they played in is the only order the
    // trick knows about.
    trick.addPlayedCard(Seat{2}, heartsTwo);
    trick.addPlayedCard(Seat{0}, heartsKing);
    trick.addPlayedCard(Seat{1}, spadesAce);

    const std::vector<PlayedCard>& played = trick.getPlayedCards();

    REQUIRE(played.size() == 3);

    REQUIRE(played[0].seat == Seat{2});
    REQUIRE(played[0].card == heartsTwo);
    REQUIRE(played[1].seat == Seat{0});
    REQUIRE(played[1].card == heartsKing);
    REQUIRE(played[2].seat == Seat{1});
    REQUIRE(played[2].card == spadesAce);
}

TEST_CASE("Trick::cardsInPlayOrder drops the seats and keeps the order", "[trick]")
{
    Card heartsTwo(Rank::Two, Suit::Hearts);
    Card spadesAce(Rank::Ace, Suit::Spades);

    Trick trick;

    SECTION("empty before anything is played")
    {
        // This is what a leader is handed: PlayContext::playedCards is empty
        // when nobody has played yet.
        REQUIRE(trick.cardsInPlayOrder().empty());
    }

    SECTION("the cards alone, in the order they were played")
    {
        trick.addPlayedCard(Seat{2}, heartsTwo);
        trick.addPlayedCard(Seat{0}, spadesAce);

        const std::vector<Card> expected{ heartsTwo, spadesAce };

        REQUIRE(trick.cardsInPlayOrder() == expected);
    }

    SECTION("grows with the trick, so a strategy sees only what has been played")
    {
        trick.addPlayedCard(Seat{2}, heartsTwo);
        REQUIRE(trick.cardsInPlayOrder().size() == 1);

        trick.addPlayedCard(Seat{0}, spadesAce);
        REQUIRE(trick.cardsInPlayOrder().size() == 2);
    }
}

TEST_CASE("Trick reports whether its lead suit and winner have been set", "[trick]")
{
    Trick trick;

    SECTION("a fresh trick has neither")
    {
        // Both getters return a placeholder before they are set, which is why
        // the has* pair is what tells a real value from it.
        REQUIRE_FALSE(trick.hasLeadSuit());
        REQUIRE_FALSE(trick.hasWinner());
    }

    SECTION("set once, and they hold what they were given")
    {
        trick.setLeadSuit(Suit::Clubs);
        trick.setWinner(Seat{2});

        REQUIRE(trick.hasLeadSuit());
        REQUIRE(trick.getLeadSuit() == Suit::Clubs);

        REQUIRE(trick.hasWinner());
        REQUIRE(trick.getWinner() == Seat{2});
    }

    SECTION("a copied trick carries both across")
    {
        // Round::addTrick() stores a copy, and the results of a whole round are
        // counted off the winners in those copies.
        trick.setLeadSuit(Suit::Diamonds);
        trick.setWinner(Seat{1});

        const Trick copy = trick;

        REQUIRE(copy.hasLeadSuit());
        REQUIRE(copy.getLeadSuit() == Suit::Diamonds);
        REQUIRE(copy.hasWinner());
        REQUIRE(copy.getWinner() == Seat{1});
    }
}

// Round owns the trick in flight, so its guards are what stand between a
// caller and a malformed trick - and the size check alone is not all of "one
// card per seat".

TEST_CASE("Round::addCardToCurrentTrick takes one card per seat", "[round]")
{
    Card heartsTwo(Rank::Two, Suit::Hearts);
    Card heartsKing(Rank::King, Suit::Hearts);
    Card spadesAce(Rank::Ace, Suit::Spades);

    Round round(1, Seat{0}, 2);
    round.resetCurrentTrick();

    round.addCardToCurrentTrick(Seat{0}, heartsTwo);

    SECTION("and refuses a second card from a seat that has already played")
    {
        // The size check does not see this: two cards on a two-seat table is a
        // full trick by count. Unguarded it ranks seat 0 twice and leaves seat
        // 1 with no card in a trick that is already closed to it.
        REQUIRE_THROWS_AS(round.addCardToCurrentTrick(Seat{0}, heartsKing), std::logic_error);

        // And the refusal left the trick as it found it, so the seat that has
        // not played still can.
        REQUIRE(round.getCurrentTrick().getPlayedCards().size() == 1);
        REQUIRE_NOTHROW(round.addCardToCurrentTrick(Seat{1}, spadesAce));
    }

    SECTION("and refuses more cards than there are seats")
    {
        round.addCardToCurrentTrick(Seat{1}, heartsKing);

        REQUIRE_THROWS_AS(round.addCardToCurrentTrick(Seat{0}, spadesAce), std::logic_error);
        REQUIRE(round.getCurrentTrick().getPlayedCards().size() == 2);
    }

    SECTION("and refuses a seat that is not at this table")
    {
        REQUIRE_THROWS_AS(round.addCardToCurrentTrick(Seat{2}, heartsKing), std::out_of_range);
    }
}
