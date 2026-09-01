#include <romanian_whist/Round.h>

#include <stdexcept>
#include <string>

namespace romanian_whist
{
Round::Round(unsigned int _trickCount,
             Seat _roundLeader,
             unsigned int seatCount,
             RoundType _type) : bets(seatCount),
                                trickCount(_trickCount),
                                type(_type),
                                trickLeaderSeat(_roundLeader),
                                roundLeaderSeat(_roundLeader)
{
    if(_roundLeader.index >= seatCount)
        throw std::out_of_range("Round::Round: round leader seat out of range");
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

void Round::setTrumpCard(Card card)
{
    trump = card;
}

std::optional<Card> Round::getTrumpCard() const
{
    return trump;
}

unsigned int Round::getTrickCount() const
{
    return trickCount;
}

void Round::setTrickLeaderSeat(Seat seat)
{
    if(seat.index >= bets.size())
        throw std::out_of_range("Round::setTrickLeaderSeat: seat out of range");

    trickLeaderSeat = seat;
}

Seat Round::getTrickLeaderSeat() const
{
    return trickLeaderSeat;
}

Seat Round::getRoundLeaderSeat() const
{
    return roundLeaderSeat;
}

const Trick& Round::getCurrentTrick() const
{
    return currentTrick;
}

void Round::resetCurrentTrick()
{
    currentTrick = Trick();
}

void Round::addCardToCurrentTrick(Seat seat, Card card)
{
    // A Card by value is always a card, so the null check this used to open
    // with has nothing left to test.
    if(seat.index >= bets.size())
        throw std::out_of_range("Round::addCardToCurrentTrick: seat out of range");

    // One card per seat, both ways round: more entries than seats would rank a
    // card twice and hand the trick to whoever played last, and a seat playing
    // twice does the same thing while leaving another seat short - which the
    // size check alone never sees.
    if(currentTrick.getPlayedCards().size() >= bets.size())
        throw std::logic_error("Round::addCardToCurrentTrick: trick already has a card from every seat");

    for(const PlayedCard& played : currentTrick.getPlayedCards())
        if(played.seat.index == seat.index)
            throw std::logic_error("Round::addCardToCurrentTrick: that seat has already "
                                   "played a card in this trick");

    // The first card sets the suit everyone else has to follow. Doing it here
    // rather than leaving it to the caller is the point of the round owning the
    // trick: there is no longer a way to add a card and forget.
    if(!currentTrick.hasLeadSuit())
        currentTrick.setLeadSuit(card.suit);

    currentTrick.addPlayedCard(seat, card);
}

void Round::finishCurrentTrick(Seat winner)
{
    if(currentTrick.getPlayedCards().empty())
        throw std::logic_error("Round::finishCurrentTrick: no trick in flight");

    currentTrick.setWinner(winner);

    // addTrick() does the range and capacity checking, and it is what makes the
    // trick count towards getTricksWon() from here on.
    addTrick(currentTrick);
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

const Trick &Round::getTrick(std::size_t index) const
{
    if(index >= tricks.size())
        throw std::out_of_range("Round::getTrick: trick " + std::to_string(index)
                                + " of a round holding " + std::to_string(tricks.size()));

    return tricks[index];
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
