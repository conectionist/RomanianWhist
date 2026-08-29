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
    std::vector<Trick> tricks;

    // Indexed by seat, sized to the table at construction. Disengaged until
    // that seat has bid.
    std::vector<std::optional<unsigned int>> bets;

    Card* trump;
    unsigned int trickCount;
    RoundType type;

    // Who leads the next trick. Reassigned to each trick's winner as the round
    // plays out, so it stops identifying who opened the round after trick one.
    Seat leader;

    // Who led the round's first trick, fixed for the round's lifetime. This is
    // the one that determines bidding order.
    Seat opener;

public:
    Round(unsigned int _trickCount, Seat _opener, unsigned int seatCount,
          RoundType _type = RoundType::Normal);

    // The trick must already have its winner set, to a seat at this table:
    // that winner is the only record of who took it (see getTricksWon()), so
    // a trick added without one is unscoreable. Throws std::logic_error and
    // std::out_of_range respectively.
    void addTrick(const Trick& trick);
    void setBet(Seat seat, unsigned int guess);
    void setTrumpCard(Card* card);
    Card* getTrumpCard();
    const Card* getTrumpCard() const;
    unsigned int getTrickCount() const;

    void setLeaderSeat(Seat seat);
    Seat getLeaderSeat() const;
    Seat getOpenerSeat() const;

    void setRoundType(RoundType _type);
    RoundType getRoundType() const;

    std::size_t getPlayedTrickCount() const;

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
