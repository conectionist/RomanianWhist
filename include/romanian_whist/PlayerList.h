#ifndef PLAYER_LIST_H
#define PLAYER_LIST_H

#include <romanian_whist/Player.h>
#include <romanian_whist/Seat.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace romanian_whist
{
class PlayerList
{
private:
    // A vector, not a list: Seat is the public way to name a player, so nothing
    // outside holds an iterator across a mutation any more, and at() stops
    // being an O(n) walk. Every player is added before the deck and the round
    // schedule are built (GameEngine::addPlayer throws afterwards), so the
    // reallocation that growing this does cannot surprise anything that has
    // already recorded a seat.
    std::vector<Player> players;

public:
    using iterator = std::vector<Player>::iterator;
    using const_iterator = std::vector<Player>::const_iterator;
    using size_type = std::vector<Player>::size_type;

    PlayerList() = default;

    void addPlayer(const std::string& playerName, std::unique_ptr<IMoveProvider> moveProvider);

    bool empty() const;
    size_type size() const;

    Player& at(size_type index);
    const Player& at(size_type index) const;

    Player& operator[](size_type index);
    const Player& operator[](size_type index) const;

    // The seat after this one, wrapping round the table.
    Seat nextSeat(Seat seat) const;

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;

    iterator end();
    const_iterator end() const;
    const_iterator cend() const;
};

} // namespace romanian_whist

#endif
