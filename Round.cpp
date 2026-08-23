#include "Round.h"

Round::Round(unsigned int _trickCount, 
             PlayerList::iterator player,
             RoundType _type) : trump(nullptr),
                                trickCount(_trickCount),
                                type(_type),
                                firstPlayer(player)
{}

void Round::addTrick(const Trick &trick)
{
    tricks.push_back(trick);
}

void Round::setBet(PlayerList::iterator player, unsigned int guess)
{
    bets[player->getName()].guess = guess;
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

void Round::setRoundType(RoundType _type)
{
    type = _type;
}

unsigned int Round::getBet(const string& playerName) const
{
    auto it = bets.find(playerName);
    if(it != bets.end())
    {
        return it->second.guess;
    }
    return 0;
}

unsigned int Round::getActual(const string& playerName) const
{
    auto it = bets.find(playerName);
    if(it != bets.end())
    {
        return it->second.actual;
    }
    return 0;
}

bool Round::hasBet(const string& playerName) const
{
    return bets.find(playerName) != bets.end();
}
