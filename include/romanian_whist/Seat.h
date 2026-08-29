#ifndef SEAT_H
#define SEAT_H

namespace romanian_whist
{
// Which chair at the table, counted from 0 in the order players were added.
// This is the engine's public currency for naming a player: a Seat is a value,
// so it can be stored in a Round or a Trick, compared, and handed to a client
// without carrying any access to the Player it names, and without dragging
// PlayerList (and through it Player and IMoveProvider) into every header that
// needs to say who did something.
//
// A struct rather than an alias for unsigned int, and constructed explicitly,
// because a seat travels next to counts that look exactly like it: placeBet()
// takes a seat and a bet, Round's constructor a trick count, a seat and a seat
// count. An alias lets any of those be passed in a seat's place, and the
// swapped call still compiles - which is precisely the mistake the iterator
// this replaced used to make impossible.
//
// index is public because a seat is an index: it keys the bets in a Round and
// the players in a PlayerList, and hiding that behind an accessor would only
// spell the same thing longer.
struct Seat
{
    unsigned int index;

    explicit constexpr Seat(unsigned int _index) : index(_index) {}
};

constexpr bool operator==(Seat left, Seat right)
{
    return left.index == right.index;
}

constexpr bool operator!=(Seat left, Seat right)
{
    return left.index != right.index;
}

} // namespace romanian_whist

#endif
