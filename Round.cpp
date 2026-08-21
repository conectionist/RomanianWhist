#include "Round.h"

Round::Round(unsigned int _handCount, 
             const Player* player,
             RoundType _type) : handCount(_handCount),
                                trump(nullptr),
                                type(_type),
                                firstPlayer(player)
{}

void Round::addHand(const Hand &hand)
{
    hands.push_back(hand);
}

void Round::addResult(Player *player, int wonHands)
{
    results.emplace_back(player, wonHands);
}

void Round::setTrumpCard(Card *card)
{
    trump = card;
}

Card *Round::getTrumpCard()
{
    return trump;
}

unsigned int Round::getHandCount()
{
    return handCount;
}

void Round::setFirstPlayer(const Player *player)
{
    firstPlayer = player;
}

const Player *Round::getFirstPlayer() const
{
    return firstPlayer;
}

void Round::setRoundType(RoundType _type)
{
    type = _type;
}
