#ifndef GAME_HARNESS_H
#define GAME_HARNESS_H

#include <romanian_whist/Card.h>
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IMoveProvider.h>
#include <romanian_whist/Scoreboard.h>
#include <romanian_whist/Seat.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace romanian_whist::test
{
struct GameHooks
{
    // Fires after a bid is chosen, before placeBet() records it - so a hook
    // sees the same forbiddenBet() the bid was chosen against.
    std::function<void(const GameEngine&, Seat seat, unsigned int bet)> onBeforeBetPlaced;

    // Fires after a card is chosen. `handBeforePlay` is a snapshot taken
    // before the move provider was asked, since Player::playCard erases the
    // chosen card from the hand as part of the same call.
    std::function<void(const std::vector<Card*>& handBeforePlay, Card* trump,
                        const Suit* leadSuit, Card* playedCard)> onBeforeCardPlayed;

    // Fires once per round, after calculateScores() and before
    // completeCurrentRound() advances the index - while the round's bets and
    // results are still live and its index hasn't moved yet.
    std::function<void(const GameEngine&)> onRoundScored;
};

// Duplicates TerminalRomanianWhist::loop()/playCurrentRoundTricks(), stripped
// of every view/renderer/pacer call. `providers.size()` is the player count,
// one provider per seat in seat order. `endWithForeheadAndHidden` and
// `all1GamesAreForehead` default to off, matching every existing caller.
GameEngine playFullGame(GameStructure structure,
                        std::vector<std::unique_ptr<IMoveProvider>> providers,
                        std::uint32_t seed,
                        const GameHooks& hooks = {},
                        bool endWithForeheadAndHidden = false,
                        bool all1GamesAreForehead = false);

std::vector<int> finalScores(const GameEngine& engine);   // seat-ordered totals

// (bid, tricksWon) per seat, per round.
using RoundRecord = std::vector<std::vector<std::pair<unsigned int, unsigned int>>>;

// Returns a GameHooks::onRoundScored hook that appends each round's record
// onto `record` as it is played. The one place this reads a round's bids and
// results, so a later API change touches this function alone, not every test
// that wants a round-by-round history.
std::function<void(const GameEngine&)> recordRoundsInto(RoundRecord& record);

} // namespace romanian_whist::test

#endif
