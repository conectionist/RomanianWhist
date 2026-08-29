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
using Seat = unsigned int;

} // namespace romanian_whist

#endif
