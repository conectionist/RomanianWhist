#include <romanian_whist/PlayerList.h>

#include <stdexcept>
#include <utility>

namespace romanian_whist
{
void PlayerList::addPlayer(const std::string& playerName, std::unique_ptr<IMoveProvider> moveProvider)
{
    players.emplace_back(playerName, std::move(moveProvider));
}

bool PlayerList::empty() const
{
    return players.empty();
}

PlayerList::size_type PlayerList::size() const
{
    return players.size();
}

Player& PlayerList::at(size_type index)
{
    if(index >= players.size())
        throw std::out_of_range("PlayerList index out of range");

    return players[index];
}

const Player& PlayerList::at(size_type index) const
{
    if(index >= players.size())
        throw std::out_of_range("PlayerList index out of range");

    return players[index];
}

Player& PlayerList::operator[](size_type index)
{
    return players[index];
}

const Player& PlayerList::operator[](size_type index) const
{
    return players[index];
}

Seat PlayerList::nextSeat(Seat seat) const
{
    if(players.empty())
        return Seat{0};

    return Seat{static_cast<unsigned int>((seat.index + 1) % players.size())};
}

PlayerList::iterator PlayerList::begin()
{
    return players.begin();
}

PlayerList::const_iterator PlayerList::begin() const
{
    return players.begin();
}

PlayerList::const_iterator PlayerList::cbegin() const
{
    return players.cbegin();
}

PlayerList::iterator PlayerList::end()
{
    return players.end();
}

PlayerList::const_iterator PlayerList::end() const
{
    return players.end();
}

PlayerList::const_iterator PlayerList::cend() const
{
    return players.cend();
}

} // namespace romanian_whist
