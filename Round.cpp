#include "Round.h"

Round::Round(unsigned int handCount_, RoundType type_) : handCount(handCount_),
                                                         trump(nullptr),
                                                         type(type_)
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

void Round::setFirstPlayer(Player *player)
{
    firstPlayer = player;
}

Player *Round::getFirstPlayer()
{
    return firstPlayer;
}
