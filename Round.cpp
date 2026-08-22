#include "Round.h"

Round::Round(unsigned int _handCount, 
             PlayerList::iterator player,
             RoundType _type) : trump(nullptr),
                                handCount(_handCount),
                                type(_type),
                                firstPlayer(player)
{}

void Round::addHand(const Hand &hand)
{
    hands.push_back(hand);
}

void Round::setBet(PlayerList::iterator player, unsigned int guess)
{
    bets[player->getName()].guess = guess;
}

void Round::setResult(PlayerList::iterator player, unsigned int wonHands)
{
    bets[player->getName()].actual = wonHands;
}

void Round::setTrumpCard(Card *card)
{
    trump = card;
}

Card *Round::getTrumpCard()
{
    return trump;
}

unsigned int Round::getHandCount() const
{
    return handCount;
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
