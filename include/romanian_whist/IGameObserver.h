#ifndef I_GAME_OBSERVER_H
#define I_GAME_OBSERVER_H

#include <romanian_whist/Card.h>
#include <romanian_whist/Seat.h>

namespace romanian_whist
{
class GameEngine;

// How a client watches a game it no longer drives. GameEngine::run() owns the
// loop and reports what happens here; IMoveProvider is still where a client
// decides what to do.
//
// Called synchronously, on the thread running the game, in the order events
// occur. Every method is a no-op by default, so a client implements only what
// it renders.
//
// Blocking inside a callback pauses the game. That is intentional - it is where
// pacing and "press Enter to continue" belong.
//
// A callback receives `const GameEngine&`, which is only safe to read on the
// thread running the game. A client whose UI lives on another thread must
// snapshot by value inside the callback and hand the snapshot across.
//
// A callback must not add or remove observers - the engine is walking that list
// to reach this call, and both throw std::logic_error rather than corrupt the
// walk. An observer that wants to detach sets a flag and lets the client detach
// it between rounds.
//
// A callback must not drive the game either: run() and playRound() throw
// std::logic_error from every callback below except onGameStarted(), which the
// engine makes while it is still idle. A round re-entered from inside one deals
// over the hands and bets the outer round is midway through playing.
// requestStop() is the one thing a callback may always do.
//
// Parameters are unnamed so that a no-op override does not have to silence an
// unused-parameter warning; each is named in the comment above it.
class IGameObserver
{
public:
    virtual ~IGameObserver() = default;

    // The game has been set up: the schedule exists and round 0 is constructed,
    // so every round-scoped accessor answers. The round has NOT been dealt yet -
    // trump and every bet are empty and getCurrentTrickNumber() is 0.
    virtual void onGameStarted(const GameEngine&) {}

    // Dealt, trump known, the in-flight trick reset. getCurrentRoundIndex() is
    // this round and getCurrentTrickNumber() is 0.
    virtual void onRoundStarted(const GameEngine&) {}

    // (engine, seat) - before the provider is asked. getActiveSeat() is that
    // seat and its bet is still empty, so getForbiddenBet() is what the bid is
    // about to be chosen against.
    virtual void onBetRequested(const GameEngine&, Seat) {}

    // (engine, seat, bet) - after the bet is recorded, so getBet(seat) is
    // engaged and getForbiddenBet() has already moved on. getActiveSeat() is
    // still that seat.
    virtual void onBetPlaced(const GameEngine&, Seat, unsigned int) {}

    // Every seat's getBet() is engaged; getActiveSeat() is empty.
    virtual void onBettingComplete(const GameEngine&) {}

    // (engine, trickNumber, leader) - after the in-flight trick is reset, so
    // getCurrentTrick() is empty and getCurrentTrickNumber() is this trick.
    virtual void onTrickStarted(const GameEngine&, unsigned int, Seat) {}

    // (engine, seat) - before the provider is asked. That seat's card is not in
    // getCurrentTrick() yet, so getCurrentTrickLeader() names who is winning
    // without it.
    virtual void onCardRequested(const GameEngine&, Seat) {}

    // (engine, seat, card) - after the card joins the in-flight trick.
    // getActiveSeat() is still that seat.
    virtual void onCardPlayed(const GameEngine&, Seat, const Card&) {}

    // (engine, winner, trickNumber) - after the completed trick is appended to
    // the round and before it is cleared. So getTricksWon(winner) INCLUDES this
    // trick, and getCurrentTrick() is still the finished trick - which is what
    // lets a client draw the completed table under "X wins the trick".
    //
    // Note getTrickLeaderSeat() has already moved to the winner. A client
    // rebuilding its table must read the seats off getCurrentTrick(), never
    // recompute them as (leader + i) % playerCount.
    virtual void onTrickWon(const GameEngine&, Seat, unsigned int) {}

    // Scored, not yet committed. This - not onRoundComplete - is the round-end
    // render hook: it is the last callback at which getCurrentRoundIndex() still
    // names the round being reported. getPlayerRoundScores() gives each seat
    // what this round was worth alongside the total it is about to fold into,
    // and getPlayerScores() the committed totals it has not folded into yet.
    virtual void onRoundScored(const GameEngine&) {}

    // Committed, and the index has already advanced - so getCurrentRoundIndex()
    // is the NEXT round, or on the last round the status is Finished and the
    // index stays put.
    virtual void onRoundComplete(const GameEngine&) {}

    // Played to the end of the schedule. Fires once.
    virtual void onGameOver(const GameEngine&) {}

    // requestStop() was honoured at a round, bid or trick boundary. Fires once,
    // instead of onGameOver(). The round it stopped in is left unscored -
    // including when the stop lands during that round's LAST trick, which is
    // played out in full and then abandoned unscored like any other.
    //
    // A stop raised during bidding is honoured before the next seat is asked to
    // bid, so the rest of the table is never prompted for a hand that is being
    // abandoned; getPhase() is Betting there, and the round has no tricks.
    //
    // A stop asked for once the last round is being scored has no boundary left
    // to land on, so it does not fire this. The flag's last read is before that
    // round is scored: from onRoundScored() the game is still InProgress, but
    // nothing reads the flag again, and by the time anything could the round is
    // committed and the game has moved to Finished. From onRoundComplete() on
    // the final round it is Finished already. Either way the game ended on its
    // own terms before the stop could be honoured, so onGameOver() reports it.
    virtual void onGameStopped(const GameEngine&) {}
};

} // namespace romanian_whist

#endif
