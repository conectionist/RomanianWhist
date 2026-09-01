#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "CardStringMaker.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/PlayContext.h>
#include <romanian_whist/strategies/IStrategy.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace romanian_whist;

// AiMoveProvider is the whole of the bridge between strategies, which reason in
// cards, and the engine, which takes an index into the hand. It is three lines,
// it runs on every AI seat on every trick, and nothing exercised it directly.
//
// Its throw is the load-bearing part. Before cards were held by value, a
// strategy returning a card the player did not hold was caught downstream by
// the engine's own legality check. That check is now structurally unreachable -
// an in-range index names a card in the hand by construction - so this throw is
// the ONLY remaining thing standing between a buggy strategy and an index that
// silently means a different card.
//
// IStrategy is a public header, so this is a real extension point rather than a
// private detail: the four bundled strategies all return a member of
// getLegalCards() and cannot trip it, but the fifth one someone writes might.

namespace
{
const Card SpadesQueen(Rank::Queen, Suit::Spades);
const Card HeartsKing(Rank::King, Suit::Hearts);
const Card ClubsSeven(Rank::Seven, Suit::Clubs);

// Answers with whatever it is told to, including things a real strategy would
// never say. This is the only way to reach the bridge's failure path: every
// bundled strategy picks out of getLegalCards() and so is incapable of it.
class FixedChoiceStrategy : public IStrategy
{
public:
    std::optional<Card> choice;

    explicit FixedChoiceStrategy(std::optional<Card> _choice) : choice(_choice) {}

    unsigned int getBestBet(const BetContext&) override { return 0; }
    std::optional<Card> getBestChoice(const PlayContext&) override { return choice; }
};

std::unique_ptr<AiMoveProvider> providerChoosing(std::optional<Card> choice)
{
    return std::make_unique<AiMoveProvider>(std::make_unique<FixedChoiceStrategy>(choice));
}
}

TEST_CASE("AiMoveProvider returns the position of the card the strategy chose", "[ai-provider]")
{
    const std::vector<Card> hand{ SpadesQueen, HeartsKing, ClubsSeven };
    const std::vector<Card> nothingPlayed;

    PlayContext context{ hand, nothingPlayed, std::nullopt, std::nullopt, 0, 0 };

    // Each position in turn, and the ends matter as much as the middle: a
    // bridge that always answered 0, or that was off by one, would still hand
    // back a legal card from a legal hand and be caught by nothing else in the
    // suite - the same blind spot that made the engine's positional-erase test
    // inadequate until it was tied to the provider's intent.
    SECTION("first card")
    {
        REQUIRE(providerChoosing(SpadesQueen)->playCard(context) == std::size_t{0});
    }

    SECTION("a card in the middle")
    {
        REQUIRE(providerChoosing(HeartsKing)->playCard(context) == std::size_t{1});
    }

    SECTION("the last card")
    {
        REQUIRE(providerChoosing(ClubsSeven)->playCard(context) == std::size_t{2});
    }
}

TEST_CASE("A strategy that invents a card is rejected, by name", "[ai-provider]")
{
    const std::vector<Card> hand{ SpadesQueen, HeartsKing };
    const std::vector<Card> nothingPlayed;

    PlayContext context{ hand, nothingPlayed, std::nullopt, std::nullopt, 0, 0 };

    // A perfectly plausible card - it simply was not dealt to this player.
    const auto provider = providerChoosing(ClubsSeven);

    REQUIRE_THROWS_AS(provider->playCard(context), std::logic_error);

    // The message names the card, which is the entire diagnostic value of
    // throwing here rather than letting an out-of-range index surface a frame
    // later inside Player::playCard, where the hand is all that is left to
    // complain about.
    REQUIRE_THROWS_WITH(provider->playCard(context),
                        Catch::Matchers::ContainsSubstring(ClubsSeven.toString()));
}

TEST_CASE("A strategy with no legal play passes the empty answer through", "[ai-provider]")
{
    const std::vector<Card> hand{ SpadesQueen };
    const std::vector<Card> nothingPlayed;

    PlayContext context{ hand, nothingPlayed, std::nullopt, std::nullopt, 0, 0 };

    // Distinct from the throw above: "I have no legal card" is a contractual
    // answer the engine turns into its own error, whereas "here is a card you
    // do not hold" is a bug in the strategy. The bridge must not collapse the
    // two - an empty answer has no card to look up and must not be reported as
    // an invented one.
    REQUIRE(providerChoosing(std::nullopt)->playCard(context) == std::nullopt);
}
