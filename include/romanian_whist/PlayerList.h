#ifndef PLAYER_LIST_H
#define PLAYER_LIST_H

#include <romanian_whist/Player.h>
#include <romanian_whist/Seat.h>

#include <cstddef>
#include <deque>
#include <memory>
#include <string>

namespace romanian_whist
{
class PlayerList
{
private:
    // A deque, not a list or a vector. Seat is the public way to name a player,
    // so nothing outside needs a list's O(n) walk to reach one - but at() and
    // operator[] still hand out Player&, and a vector's reallocation on the next
    // addPlayer() would leave every such reference dangling. A deque indexes in
    // O(1) like a vector and keeps references and pointers to the players
    // already in it valid across addPlayer(), like a list.
    //
    // Its iterators are the one thing addPlayer() still invalidates. Nothing
    // holds one across a call: begin()/end() exist for immediate traversal of
    // the whole table, and the seats are all added in one go by
    // GameEngine::start(), which is the only caller and never adds another.
    std::deque<Player> players;

public:
    using iterator = std::deque<Player>::iterator;
    using const_iterator = std::deque<Player>::const_iterator;
    using size_type = std::deque<Player>::size_type;

    PlayerList() = default;

    void addPlayer(const std::string& playerName, std::unique_ptr<IMoveProvider> moveProvider);

    bool empty() const;
    size_type size() const;

    Player& at(size_type index);
    const Player& at(size_type index) const;

    Player& operator[](size_type index);
    const Player& operator[](size_type index) const;

    // The seat after this one, wrapping round the table. Throws
    // std::out_of_range for a seat that is not at this table, and for any seat
    // at all while the table is empty: wrapping such a seat into range would
    // hand back a valid-looking neighbour and lose the mistake.
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
