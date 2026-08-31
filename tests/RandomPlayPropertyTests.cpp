#include <catch2/catch_test_macros.hpp>

#include "GameHarness.h"

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/Player.h>
#include <romanian_whist/strategies/RandomCardStrategy.h>

#include <algorithm>

using namespace romanian_whist;
using namespace romanian_whist::test;

namespace
{
// Fixed, committed range - not std::random_device - so a failure reproduces
// locally with the seed printed via INFO below. Breadth across seeds is the
// point here, not which seeds are used.
constexpr std::uint32_t kPropertySeedCount = 2000;

// Independently reimplements the "follow suit, else trump, else anything"
// rule, rather than calling CardValidator::getLegalCards() - the strategy
// under test already calls that function to choose its card, so re-deriving
// "expected" from the very function that produced "actual" would make this
// check pass regardless of whether that function is correct.
//
// The engine now checks the same thing for itself before it accepts a card
// (see MoveValidationTests). That does not make this redundant: the engine's
// check goes through getLegalCards() too, so this is still the only assertion
// in the suite that would survive that function being wrong.
bool followsTheRules(const std::vector<Card*>& hand, const Card* trump, const Suit* leadSuit, const Card* playedCard)
{
    if(std::find(hand.begin(), hand.end(), playedCard) == hand.end())
        return false;

    if(leadSuit == nullptr)
        return true;

    const auto holdsSuit = [&](Suit suit)
    {
        return std::any_of(hand.begin(), hand.end(), [suit](Card* card) { return card->suit == suit; });
    };

    if(holdsSuit(*leadSuit))
        return playedCard->suit == *leadSuit;

    if(trump && holdsSuit(trump->suit))
        return playedCard->suit == trump->suit;

    return true;
}

// Every invariant the old GameHooks checked, re-expressed against the
// callbacks the engine actually publishes.
//
// Two of them have to move to a different callback than they used to sit on,
// and the reason is the same in both cases: a hook fired *before* the engine
// recorded something, whereas an observer is told *after*. So whatever has to
// be read from the pre-move state gets captured at the matching *Requested
// callback and checked at the *Placed / *Played one.
class RuleChecker : public IGameObserver
{
public:
    explicit RuleChecker(unsigned int playerCount) : tricksWonBySeat(playerCount, 0) {}

    void onBetRequested(const GameEngine& engine, Seat) override
    {
        // Captured here because by onBetPlaced() the bid is already recorded,
        // so getForbiddenBet() has moved on to the next bidder - or gone empty.
        // Asking then would test the wrong round state and quietly pass.
        forbiddenBet = engine.getForbiddenBet();
        trickCount = engine.getCurrentRoundTrickCount();
    }

    void onBetPlaced(const GameEngine&, Seat, unsigned int bet) override
    {
        REQUIRE(bet <= trickCount);
        REQUIRE_FALSE((forbiddenBet && bet == *forbiddenBet));
    }

    void onCardRequested(const GameEngine& engine, Seat seat) override
    {
        // Snapshotted before the provider is asked: Player::playCard erases the
        // chosen card from the hand as part of the same call, so by
        // onCardPlayed() the hand no longer contains the card being judged.
        handBeforePlay = engine.getPlayers().at(seat.index).getHand();
        trumpBeforePlay = engine.getCurrentTrumpCard();

        const Trick& trick = engine.getCurrentTrick();
        hasLeadSuit = trick.hasLeadSuit();

        if(hasLeadSuit)
            leadSuit = trick.getLeadSuit();
    }

    void onCardPlayed(const GameEngine&, Seat, const Card& card) override
    {
        REQUIRE(followsTheRules(handBeforePlay,
                                trumpBeforePlay,
                                hasLeadSuit ? &leadSuit : nullptr,
                                &card));
    }

    void onTrickWon(const GameEngine&, Seat winner, unsigned int) override
    {
        // Tallied independently so the round's own results have something to be
        // held against. A sum over the seats would not do: addTrick() rejects a
        // trick with no winner or an off-table one, so that total can only ever
        // equal getPlayedTrickCount(), and it stays right even if the round
        // credits every trick to the wrong seat.
        tricksWonBySeat[winner.index]++;
    }

    void onRoundScored(const GameEngine& engine) override
    {
        const Round& round = engine.getCurrentRound();

        for(unsigned int i = 0 ; i < engine.getPlayerCount() ; i++)
            REQUIRE(round.getTricksWon(Seat{i}) == tricksWonBySeat[i]);

        REQUIRE(round.getPlayedTrickCount() == engine.getCurrentRoundTrickCount());

        std::fill(tricksWonBySeat.begin(), tricksWonBySeat.end(), 0u);
    }

private:
    std::vector<unsigned int> tricksWonBySeat;

    std::optional<unsigned int> forbiddenBet;
    unsigned int trickCount = 0;

    std::vector<Card*> handBeforePlay;
    const Card* trumpBeforePlay = nullptr;
    bool hasLeadSuit = false;
    Suit leadSuit{};
};
}

TEST_CASE("Random play never violates the rules", "[property]")
{
    for(std::uint32_t seed = 0 ; seed < kPropertySeedCount ; seed++)
    {
        INFO("seed = " << seed);

        const unsigned int playerCount = 2 + (seed % 5);
        const GameStructure structure = (seed % 2 == 0) ? GameStructure::S_181 : GameStructure::S_818;

        std::vector<std::unique_ptr<IMoveProvider>> providers;
        for(unsigned int seat = 0 ; seat < playerCount ; seat++)
        {
            const std::uint32_t providerSeed = seed * playerCount + seat;
            providers.push_back(std::make_unique<AiMoveProvider>(
                std::make_unique<RandomCardStrategy>(providerSeed)));
        }

        RuleChecker checker(playerCount);

        playFullGame(structure, std::move(providers), seed, { &checker });
    }
}
