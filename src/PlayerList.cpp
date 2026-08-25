#include <romanian_whist/PlayerList.h>

#include <iterator>
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

    auto player = players.begin();
    std::advance(player, index);

    return *player;
}

const Player& PlayerList::at(size_type index) const
{
    if(index >= players.size())
        throw std::out_of_range("PlayerList index out of range");

    auto player = players.begin();
    std::advance(player, index);

    return *player;
}

Player& PlayerList::operator[](size_type index)
{
    auto player = players.begin();
    std::advance(player, index);

    return *player;
}

const Player& PlayerList::operator[](size_type index) const
{
    auto player = players.begin();
    std::advance(player, index);

    return *player;
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

PlayerList::iterator PlayerList::first()
{
    return players.begin();
}

PlayerList::const_iterator PlayerList::first() const
{
    return players.begin();
}

PlayerList::iterator PlayerList::next(iterator current)
{
    if(players.empty())
        return players.end();

    if(current == players.end())
        return players.begin();

    ++current;

    if(current == players.end())
        return players.begin();

    return current;
}

PlayerList::const_iterator PlayerList::next(const_iterator current) const
{
    if(players.empty())
        return players.end();

    if(current == players.end())
        return players.begin();

    ++current;

    if(current == players.end())
        return players.begin();

    return current;
}

PlayerList::iterator PlayerList::advanceCircular(iterator current, size_type steps)
{
    if(players.empty())
        return players.end();

    auto result = current;

    for(size_type i = 0 ; i < steps ; i++)
        result = next(result);

    return result;
}

PlayerList::const_iterator PlayerList::advanceCircular(const_iterator current, size_type steps) const
{
    if(players.empty())
        return players.end();

    auto result = current;

    for(size_type i = 0 ; i < steps ; i++)
        result = next(result);

    return result;
}

} // namespace romanian_whist
