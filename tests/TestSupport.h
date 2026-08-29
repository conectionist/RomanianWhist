#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <romanian_whist/GameEngine.h>
#include <romanian_whist/PlayerList.h>

namespace romanian_whist::test
{
// By address, not by name: two players may share a name, and list nodes do
// not move. Mirrors TerminalRomanianWhist::seatOf, since there is no
// dedicated Seat type yet.
inline unsigned int seatOf(const GameEngine& engine, PlayerList::const_iterator player)
{
    const auto& players = engine.getPlayers();

    for(unsigned int i = 0 ; i < players.size() ; i++)
    {
        if(&players.at(i) == &*player)
            return i;
    }

    return 0;
}

} // namespace romanian_whist::test

#endif
