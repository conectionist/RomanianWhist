#include <catch2/catch_test_macros.hpp>

#include <romanian_whist/CardValidator.h>

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
        std::vector<Card*> hand{ &heartsTwo, &spadesAce, &clubsFive };

        const auto legal = validator.getLegalCards(hand, nullptr, nullptr);

        REQUIRE(legal == hand);
    }

    SECTION("must follow lead suit when held")
    {
        std::vector<Card*> hand{ &heartsTwo, &heartsKing, &spadesAce };
        const Suit leadSuit = Suit::Hearts;

        const auto legal = validator.getLegalCards(hand, nullptr, &leadSuit);

        REQUIRE(legal == std::vector<Card*>{ &heartsTwo, &heartsKing });
    }

    SECTION("must trump when void in lead and holding trump")
    {
        std::vector<Card*> hand{ &spadesAce, &clubsFive };
        const Suit leadSuit = Suit::Hearts;
        Card trump(Rank::Two, Suit::Clubs);

        const auto legal = validator.getLegalCards(hand, &trump, &leadSuit);

        REQUIRE(legal == std::vector<Card*>{ &clubsFive });
    }

    SECTION("free discard when void in both lead and trump")
    {
        std::vector<Card*> hand{ &spadesAce, &diamondsFive };
        const Suit leadSuit = Suit::Hearts;
        Card trump(Rank::Two, Suit::Clubs);

        const auto legal = validator.getLegalCards(hand, &trump, &leadSuit);

        REQUIRE(legal == hand);
    }

    SECTION("overtrumping is not required")
    {
        Card trumpLow(Rank::Three, Suit::Diamonds);
        Card trumpHigh(Rank::King, Suit::Diamonds);
        std::vector<Card*> hand{ &trumpLow, &trumpHigh, &spadesAce };
        const Suit leadSuit = Suit::Hearts;
        Card trump(Rank::Two, Suit::Diamonds);

        const auto legal = validator.getLegalCards(hand, &trump, &leadSuit);

        REQUIRE(legal == std::vector<Card*>{ &trumpLow, &trumpHigh });
    }

    SECTION("empty hand yields empty legal cards")
    {
        std::vector<Card*> hand;
        const Suit leadSuit = Suit::Hearts;

        const auto legal = validator.getLegalCards(hand, nullptr, &leadSuit);

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

        REQUIRE(CardValidator::beats(candidate, best, leadSuit, &trumpCard));
    }

    SECTION("higher trump beats lower trump")
    {
        Card trumpCard(Rank::Two, Suit::Clubs);
        Card candidate(Rank::King, Suit::Clubs);
        Card best(Rank::Three, Suit::Clubs);

        REQUIRE(CardValidator::beats(candidate, best, leadSuit, &trumpCard));
    }

    SECTION("lead suit beats off-suit")
    {
        Card candidate(Rank::Two, Suit::Hearts);
        Card best(Rank::Ace, Suit::Spades);

        REQUIRE(CardValidator::beats(candidate, best, leadSuit, nullptr));
    }

    SECTION("higher rank wins within a suit")
    {
        Card candidate(Rank::King, Suit::Hearts);
        Card best(Rank::Queen, Suit::Hearts);

        REQUIRE(CardValidator::beats(candidate, best, leadSuit, nullptr));
    }

    SECTION("two off-suit discards leave the incumbent ahead")
    {
        Card candidate(Rank::Ace, Suit::Spades);
        Card best(Rank::Two, Suit::Clubs);

        REQUIRE_FALSE(CardValidator::beats(candidate, best, leadSuit, nullptr));
    }
}
