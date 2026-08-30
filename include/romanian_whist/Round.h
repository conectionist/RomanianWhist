#ifndef ROUND_H
#define ROUND_H

#include <romanian_whist/RoundType.h>
#include <romanian_whist/Seat.h>
#include <romanian_whist/Trick.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace romanian_whist
{
class Round
{
private:
    // Completed tricks, each with its winner set. getTricksWon() is counted off
    // these, so this is the round's only record of who took what.
    std::vector<Trick> tricks;

    // The trick being played right now. Kept here rather than in a loop local
    // so that an observer can read a half-played trick, and so that a future
    // non-blocking playStep() is an additive change rather than a rewrite.
    Trick currentTrick;

    // Indexed by seat, sized to the table at construction. Disengaged until
    // that seat has bid.
    std::vector<std::optional<unsigned int>> bets;

    Card* trump;
    unsigned int trickCount;
    RoundType type;

    // Who leads the next trick. Reassigned to each trick's winner as the round
    // plays out, so it stops identifying who opened the round after trick one.
    Seat trickLeaderSeat;

    // Who led the round's first trick, fixed for the round's lifetime. This is
    // the one that determines bidding order.
    //
    // Named after the accessor that exposes it, and deliberately not "leader":
    // a bare "leader" reads as the moving one above, and confusing the two
    // draws every card against the wrong seat.
    Seat roundLeaderSeat;

public:
    // `_roundLeader` must name one of the `seatCount` seats, or this throws
    // std::out_of_range: it also seeds the trick leader, so an off-table value
    // would put the round's very first trick out of turn.
    Round(unsigned int _trickCount, Seat _roundLeader, unsigned int seatCount,
          RoundType _type = RoundType::Normal);

    // The trick must already have its winner set, to a seat at this table:
    // that winner is the only record of who took it (see getTricksWon()), so
    // a trick added without one is unscoreable. Throws std::logic_error and
    // std::out_of_range respectively, and std::logic_error again once the
    // round already holds the trickCount tricks it was dealt for.
    void addTrick(const Trick& trick);
    void setBet(Seat seat, unsigned int guess);
    void setTrumpCard(Card* card);
    Card* getTrumpCard();
    const Card* getTrumpCard() const;
    unsigned int getTrickCount() const;

    // Who leads the next trick: moves to each trick's winner. Throws
    // std::out_of_range for a seat that is not at this table, which would
    // otherwise silently rotate the turn order at the next trick.
    void setTrickLeaderSeat(Seat seat);
    Seat getTrickLeaderSeat() const;

    // Who opened the round, fixed for its lifetime. This is what bidding order
    // is measured from, and it is NOT the same question as getTrickLeaderSeat()
    // from the first won trick onwards.
    Seat getRoundLeaderSeat() const;

    void setRoundType(RoundType _type);
    RoundType getRoundType() const;

    std::size_t getPlayedTrickCount() const;

    // The trick in flight. Empty between the deal and the first card of trick
    // one; from then on it holds whatever has been played so far - and, once
    // finishCurrentTrick() has run, the completed trick, until the next
    // resetCurrentTrick() clears it.
    const Trick& getCurrentTrick() const;

    // Starts a fresh trick. Called at the deal as well as at each trick, so a
    // round never opens holding the previous one's cards.
    void resetCurrentTrick();

    // Appends to the trick in flight, setting the lead suit from the first
    // card. Throws std::out_of_range for a seat that is not at this table, and
    // std::logic_error if the card is null, if the trick is already full, or if
    // that seat has already played in it - one card per seat, so a trick can
    // never rank one seat twice while another never plays.
    void addCardToCurrentTrick(Seat seat, Card* card);

    // Names the winner of the trick in flight and files it among the completed
    // ones. Deliberately does NOT clear it: getTricksWon() must already count
    // this trick, and getCurrentTrick() must still return it, at the moment
    // observers are told the trick was won. resetCurrentTrick() is what clears.
    void finishCurrentTrick(Seat winner);

    // Disengaged until that seat has bid, which is what tells a genuine bid of
    // zero apart from no bid at all.
    std::optional<unsigned int> getBet(Seat seat) const;

    // Counted from the tricks stored so far, rather than tracked alongside
    // them: every completed trick is added with its winner, so this is already
    // recorded and a second copy could only disagree with it. A trick in
    // progress has not been added yet, so it is not counted until it is won.
    unsigned int getTricksWon(Seat seat) const;
};

} // namespace romanian_whist

#endif
