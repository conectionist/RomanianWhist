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
{}

void Round::addTrick(const Trick &trick)
{
    tricks.push_back(trick);
}

void Round::setBet(Seat seat, unsigned int guess)
{
    if(seat >= bets.size())
        throw std::out_of_range("Round::setBet: seat out of range");

    bets[seat] = guess;
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
    if(seat >= bets.size())
        throw std::out_of_range("Round::getBet: seat out of range");

    return bets[seat];
}

unsigned int Round::getTricksWon(Seat seat) const
{
    if(seat >= bets.size())
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
