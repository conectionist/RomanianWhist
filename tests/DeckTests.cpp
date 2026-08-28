#include <catch2/catch_test_macros.hpp>

#include "GameHarness.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/Deck.h>
#include <romanian_whist/strategies/DuckingStrategy.h>
#include <romanian_whist/strategies/FirstCardStrategy.h>
#include <romanian_whist/strategies/LowRiskStrategy.h>

#include <random>

using namespace romanian_whist;
using namespace romanian_whist::test;

TEST_CASE("Deck exposes size and const iteration", "[deck]")
{
    Deck deck;

    REQUIRE(deck.size() == 0);
    REQUIRE(deck.begin() == deck.end());

    deck.addCard(Card(Rank::Ace, Suit::Hearts));
    deck.addCard(Card(Rank::Two, Suit::Spades));
    deck.addCard(Card(Rank::King, Suit::Clubs));

    REQUIRE(deck.size() == 3);
    REQUIRE(static_cast<std::size_t>(std::distance(deck.begin(), deck.end())) == deck.size());
    REQUIRE(deck.begin()->rank == Rank::Ace);
}

TEST_CASE("Deck::shuffle is a portable, seeded Fisher-Yates", "[deck]")
{
    // Frozen against a fixed seed. Verified by independently reimplementing
    // the same rejection-sampling Fisher-Yates in a standalone scratch
    // program against std::mt19937(12345) and confirming every swap
    // (deck[12]<->deck[8], deck[11]<->deck[9], ..., deck[1]<->deck[1])
    // before this was written down - not pasted from the code under test.
    constexpr std::uint32_t seed = 12345;

    Deck deck;
    for(int r = static_cast<int>(Rank::Two) ; r <= static_cast<int>(Rank::Ace) ; r++)
        deck.addCard(Card(static_cast<Rank>(r), Suit::Hearts));

    std::mt19937 generator(seed);
    deck.shuffle(generator);

    std::vector<Rank> order;
    for(const Card& card : deck)
        order.push_back(card.rank);

    // clang-format off
    const std::vector<Rank> expected{
        Rank::King, Rank::Six, Rank::Ace, Rank::Seven, Rank::Eight,
        Rank::Four, Rank::Nine, Rank::Three, Rank::Queen, Rank::Five,
        Rank::Two, Rank::Jack, Rank::Ten
    };
    // clang-format on

    REQUIRE(order == expected);

    std::mt19937 generatorAgain(seed);
    Deck deckAgain;
    for(int r = static_cast<int>(Rank::Two) ; r <= static_cast<int>(Rank::Ace) ; r++)
        deckAgain.addCard(Card(static_cast<Rank>(r), Suit::Hearts));
    deckAgain.shuffle(generatorAgain);

    std::vector<Rank> orderAgain;
    for(const Card& card : deckAgain)
        orderAgain.push_back(card.rank);

    REQUIRE(order == orderAgain);
}

TEST_CASE("Two engines given the same seed play an identical game", "[deck]")
{
    // Determinism at the deck level (above) doesn't by itself prove the
    // engine as a whole is a pure function of its seed - dealing, bidding
    // and trick play all sit between the shuffle and the final scores.
    constexpr std::uint32_t seed = 777;

    auto buildProviders = []
    {
        std::vector<std::unique_ptr<IMoveProvider>> providers;
        providers.push_back(std::make_unique<AiMoveProvider>(std::make_unique<FirstCardStrategy>()));
        providers.push_back(std::make_unique<AiMoveProvider>(std::make_unique<LowRiskStrategy>()));
        providers.push_back(std::make_unique<AiMoveProvider>(std::make_unique<DuckingStrategy>()));
        return providers;
    };

    RoundRecord recordA;
    GameHooks hooksA;
    hooksA.onRoundScored = recordRoundsInto(recordA);
    GameEngine engineA = playFullGame(GameStructure::S_181, buildProviders(), seed, hooksA);

    RoundRecord recordB;
    GameHooks hooksB;
    hooksB.onRoundScored = recordRoundsInto(recordB);
    GameEngine engineB = playFullGame(GameStructure::S_181, buildProviders(), seed, hooksB);

    REQUIRE(finalScores(engineA) == finalScores(engineB));
    REQUIRE(recordA == recordB);
}
