#ifndef PLAYER_LIST_H
#define PLAYER_LIST_H

#include "Player.h"

#include <cstddef>
#include <list>
#include <string>
#include <vector>

using std::list;
using std::string;
using std::vector;

class PlayerList
{
private:
    list<Player> players;

public:
    using iterator = list<Player>::iterator;
    using const_iterator = list<Player>::const_iterator;
    using size_type = list<Player>::size_type;

    PlayerList() = default;
    explicit PlayerList(const vector<string>& playerNames);

    void addPlayer(Player&& player);
    void addPlayer(const string& playerName);
    void createPlayers(const vector<string>& playerNames);
    void clear();

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

#endif
