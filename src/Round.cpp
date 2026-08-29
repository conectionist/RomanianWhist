#include <romanian_whist/Round.h>

#include <stdexcept>

namespace romanian_whist
{
Round::Round(unsigned int _trickCount,
             Seat _opener,
             unsigned int seatCount,
             RoundType _type) : bets(seatCount),
                                trump(nullptr),
                                trickCount(_trickCount),
                                type(_type),
                                leader(_opener),
                                opener(_opener)
{
    if(_opener.index >= seatCount)
        throw std::out_of_range("Round::Round: opener seat out of range");
}

void Round::addTrick(const Trick &trick)
{
    // Tricks won are counted off the winners stored here, so a trick with no
    // winner - or one naming a seat that is not at this table - would not be
    // scored to anybody, and the round would quietly come up short. Reject it
    // where it enters rather than let calculateScores() report a 0 that looks
    // like a real result.
    if(!trick.hasWinner())
        throw std::logic_error("Round::addTrick: trick has no winner");

    if(trick.getWinner().index >= bets.size())
        throw std::out_of_range("Round::addTrick: winning seat out of range");

    // A round is dealt for exactly trickCount tricks, and the results read off
    // these (see getTricksWon()). One more would make the tricks won add up to
    // more than were ever played, which nothing downstream is in a position to
    // notice - calculateScores() would simply score the inflated total.
    if(tricks.size() >= trickCount)
        throw std::logic_error("Round::addTrick: round already has all its tricks");

    tricks.push_back(trick);
}

void Round::setBet(Seat seat, unsigned int guess)
{
    if(seat.index >= bets.size())
        throw std::out_of_range("Round::setBet: seat out of range");

    bets[seat.index] = guess;
}

void Round::setTrumpCard(Card *card)
{
    trump = card;
}

Card *Round::getTrumpCard()
{
    return trump;
}

const Card *Round::getTrumpCard() const
{
    return trump;
}

unsigned int Round::getTrickCount() const
{
    return trickCount;
}

void Round::setLeaderSeat(Seat seat)
{
    if(seat.index >= bets.size())
        throw std::out_of_range("Round::setLeaderSeat: seat out of range");

    leader = seat;
}

Seat Round::getLeaderSeat() const
{
    return leader;
}

Seat Round::getOpenerSeat() const
{
    return opener;
}

void Round::setRoundType(RoundType _type)
{
    type = _type;
}

RoundType Round::getRoundType() const
{
    return type;
}

std::size_t Round::getPlayedTrickCount() const
{
    return tricks.size();
}

std::optional<unsigned int> Round::getBet(Seat seat) const
{
    if(seat.index >= bets.size())
        throw std::out_of_range("Round::getBet: seat out of range");

    return bets[seat.index];
}

unsigned int Round::getTricksWon(Seat seat) const
{
    if(seat.index >= bets.size())
        throw std::out_of_range("Round::getTricksWon: seat out of range");

    unsigned int won = 0;

    for(const Trick& trick : tricks)
    {
        if(trick.hasWinner() && trick.getWinner() == seat)
            won++;
    }

    return won;
}

} // namespace romanian_whist
