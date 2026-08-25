#ifndef PLAYER_LIST_H
#define PLAYER_LIST_H

#include <romanian_whist/Player.h>

#include <cstddef>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace romanian_whist
{
class PlayerList
{
private:
    std::list<Player> players;

public:
    using iterator = std::list<Player>::iterator;
    using const_iterator = std::list<Player>::const_iterator;
    using size_type = std::list<Player>::size_type;

    PlayerList() = default;

    void addPlayer(const std::string& playerName, std::unique_ptr<IMoveProvider> moveProvider);

    bool empty() const;
    size_type size() const;

    Player& at(size_type index);
    const Player& at(size_type index) const;

    Player& operator[](size_type index);
    const Player& operator[](size_type index) const;

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;

    iterator end();
    const_iterator end() const;
    const_iterator cend() const;

    iterator first();
    const_iterator first() const;

    iterator next(iterator current);
    const_iterator next(const_iterator current) const;

    iterator advanceCircular(iterator current, size_type steps);
    const_iterator advanceCircular(const_iterator current, size_type steps) const;
};

} // namespace romanian_whist

#endif
