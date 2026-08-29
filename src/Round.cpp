#include <romanian_whist/Round.h>

namespace romanian_whist
{
Round::Round(unsigned int _trickCount, 
             PlayerList::iterator player,
             RoundType _type) : trump(nullptr),
                                trickCount(_trickCount),
                                type(_type),
                                firstPlayer(player),
                                openingPlayer(player)
{}

void Round::addTrick(const Trick &trick)
{
    tricks.push_back(trick);
}

void Round::setBet(PlayerList::iterator player, unsigned int guess)
{
    Bet& bet = bets[player->getName()];
    bet.guess = guess;
    bet.guessSet = true;
}

void Round::setResult(PlayerList::iterator player, unsigned int wonTricks)
{
    bets[player->getName()].actual = wonTricks;
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

void Round::setFirstPlayer(PlayerList::iterator player)
{
    firstPlayer = player;
}

PlayerList::iterator Round::getFirstPlayer() const
{
    return firstPlayer;
}

PlayerList::iterator Round::getOpeningPlayer() const
{
    return openingPlayer;
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

unsigned int Round::getBet(const std::string& playerName) const
{
    auto it = bets.find(playerName);
    if(it != bets.end())
    {
        return it->second.guess;
    }
    return 0;
}

unsigned int Round::getActual(const std::string& playerName) const
{
    auto it = bets.find(playerName);
    if(it != bets.end())
    {
        return it->second.actual;
    }
    return 0;
}

bool Round::hasBet(const std::string& playerName) const
{
    auto it = bets.find(playerName);
    return it != bets.end() && it->second.guessSet;
}

} // namespace romanian_whist
