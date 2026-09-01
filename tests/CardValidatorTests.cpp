#include <catch2/catch_test_macros.hpp>

#include "CardStringMaker.h"

#include <romanian_whist/CardValidator.h>

#include <optional>
#include <vector>

using namespace romanian_whist;

TEST_CASE("CardValidator::getLegalCards", "[card-validator]")
{
    CardValidator validator;

    Card heartsTwo(Rank::Two, Suit::Hearts);
    Card heartsKing(Rank::King, Suit::Hearts);
    Card spadesAce(Rank::Ace, Suit::Spades);
    Card clubsFive(Rank::Five, Suit::Clubs);
    Card diamondsFive(Rank::Five, Suit::Diamonds);
    Card diamondsSix(Rank::Six, Suit::Diamonds);

    SECTION("leading returns the whole hand")
    {
        std::vector<Card> hand{ heartsTwo, spadesAce, clubsFive };

        const auto legal = validator.getLegalCards(hand, std::nullopt, std::nullopt);

        REQUIRE(legal == hand);
    }

    SECTION("must follow lead suit when held")
    {
        std::vector<Card> hand{ heartsTwo, heartsKing, spadesAce };
        const Suit leadSuit = Suit::Hearts;

        const auto legal = validator.getLegalCards(hand, std::nullopt, leadSuit);

        REQUIRE(legal == std::vector<Card> { heartsTwo, heartsKing });
    }

    SECTION("must trump when void in lead and holding trump")
    {
        std::vector<Card> hand{ spadesAce, clubsFive };
        const Suit leadSuit = Suit::Hearts;
        Card trump(Rank::Two, Suit::Clubs);

        const auto legal = validator.getLegalCards(hand, trump, leadSuit);

        REQUIRE(legal == std::vector<Card> { clubsFive });
    }

    SECTION("free discard when void in both lead and trump")
    {
        std::vector<Card> hand{ spadesAce, diamondsFive };
        const Suit leadSuit = Suit::Hearts;
        Card trump(Rank::Two, Suit::Clubs);

        const auto legal = validator.getLegalCards(hand, trump, leadSuit);

        REQUIRE(legal == hand);
    }

    SECTION("overtrumping is not required")
    {
        Card trumpLow(Rank::Three, Suit::Diamonds);
        Card trumpHigh(Rank::King, Suit::Diamonds);
        std::vector<Card> hand{ trumpLow, trumpHigh, spadesAce };
        const Suit leadSuit = Suit::Hearts;
        Card trump(Rank::Two, Suit::Diamonds);

        const auto legal = validator.getLegalCards(hand, trump, leadSuit);

        REQUIRE(legal == std::vector<Card> { trumpLow, trumpHigh });
    }

    SECTION("empty hand yields empty legal cards")
    {
        std::vector<Card> hand;
        const Suit leadSuit = Suit::Hearts;

        const auto legal = validator.getLegalCards(hand, std::nullopt, leadSuit);

        REQUIRE(legal.empty());
    }
}

TEST_CASE("CardValidator::beats", "[card-validator]")
{
    const Suit leadSuit = Suit::Hearts;

    SECTION("trump beats plain")
    {
        Card trumpCard(Rank::Two, Suit::Clubs);
        Card candidate(Rank::Two, Suit::Clubs);
        Card best(Rank::Ace, Suit::Hearts);

        REQUIRE(CardValidator::beats(candidate, best, leadSuit, trumpCard));
    }

    SECTION("higher trump beats lower trump")
    {
        Card trumpCard(Rank::Two, Suit::Clubs);
        Card candidate(Rank::King, Suit::Clubs);
        Card best(Rank::Three, Suit::Clubs);

        REQUIRE(CardValidator::beats(candidate, best, leadSuit, trumpCard));
    }

    SECTION("lead suit beats off-suit")
    {
        Card candidate(Rank::Two, Suit::Hearts);
        Card best(Rank::Ace, Suit::Spades);

        REQUIRE(CardValidator::beats(candidate, best, leadSuit, std::nullopt));
    }

    SECTION("higher rank wins within a suit")
    {
        Card candidate(Rank::King, Suit::Hearts);
        Card best(Rank::Queen, Suit::Hearts);

        REQUIRE(CardValidator::beats(candidate, best, leadSuit, std::nullopt));
    }

    SECTION("two off-suit discards leave the incumbent ahead")
    {
        Card candidate(Rank::Ace, Suit::Spades);
        Card best(Rank::Two, Suit::Clubs);

        REQUIRE_FALSE(CardValidator::beats(candidate, best, leadSuit, std::nullopt));
    }
}

TEST_CASE("CardValidator::getWinningCard", "[card-validator]")
{
    // The ranking half of what GameEngine::determineTrickWinner() used to be
    // tested for directly. That method is the engine's own business now, but
    // the rule it applies is this one, and it is still worth pinning where it
    // can be asserted without a whole game around it.
    Card heartsTwo(Rank::Two, Suit::Hearts);
    Card heartsKing(Rank::King, Suit::Hearts);
    Card spadesAce(Rank::Ace, Suit::Spades);
    Card clubsThree(Rank::Three, Suit::Clubs);

    SECTION("an empty trick has no winning card")
    {
        REQUIRE(CardValidator::getWinningCard({}, Suit::Hearts, std::nullopt) == std::nullopt);
    }

    SECTION("the highest card of the lead suit wins")
    {
        const std::vector<Card> played{ heartsTwo, heartsKing };

        REQUIRE(CardValidator::getWinningCard(played, Suit::Hearts, std::nullopt) == heartsKing);
    }

    SECTION("an off-suit card does not win, however high")
    {
        const std::vector<Card> played{ heartsTwo, spadesAce };

        REQUIRE(CardValidator::getWinningCard(played, Suit::Hearts, std::nullopt) == heartsTwo);
    }

    SECTION("a trump beats the lead suit")
    {
        Card trumpCard(Rank::Two, Suit::Clubs);
        const std::vector<Card> played{ heartsKing, clubsThree };

        REQUIRE(CardValidator::getWinningCard(played, Suit::Hearts, trumpCard) == clubsThree);
    }

    SECTION("it ranks a partly played trick, which is what the live highlight needs")
    {
        // A client shows who is currently winning as the cards go down, so this
        // is asked once per card rather than once per trick.
        std::vector<Card> played{ heartsTwo };
        REQUIRE(CardValidator::getWinningCard(played, Suit::Hearts, std::nullopt) == heartsTwo);

        played.push_back(heartsKing);
        REQUIRE(CardValidator::getWinningCard(played, Suit::Hearts, std::nullopt) == heartsKing);

        // The last card cannot follow, so the standing winner is unchanged.
        played.push_back(spadesAce);
        REQUIRE(CardValidator::getWinningCard(played, Suit::Hearts, std::nullopt) == heartsKing);
    }
}

TEST_CASE("Card equality is rank and suit", "[card]")
{
    // Cards are held by value throughout, so equality is what a hand, a trick
    // and a round use to answer "is this the card that was played?". A deck
    // holds no duplicates, which is what makes this identity too.
    REQUIRE(Card(Rank::Queen, Suit::Spades) == Card(Rank::Queen, Suit::Spades));
    REQUIRE_FALSE(Card(Rank::Queen, Suit::Spades) == Card(Rank::Queen, Suit::Hearts));
    REQUIRE_FALSE(Card(Rank::Queen, Suit::Spades) == Card(Rank::King, Suit::Spades));
}
