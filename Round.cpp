#include "Round.h"
#include "Player.h"

Round::Round(unsigned int _handCount, 
             Player* player,
             RoundType _type) : trump(nullptr),
                                handCount(_handCount),
                                type(_type),
                                firstPlayer(player)
{}

void Round::addHand(const Hand &hand)
{
    hands.push_back(hand);
}

void Round::setBet(Player *player, unsigned int guess)
{
    bets[player->getName()].guess = guess;
}

void Round::setResult(Player *player, unsigned int wonHands)
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

unsigned int Round::getHandCount()
{
    return handCount;
}

void Round::setFirstPlayer(Player *player)
{
    firstPlayer = player;
}

Player *Round::getFirstPlayer() const
{
    return firstPlayer;
}

void Round::setRoundType(RoundType _type)
{
    type = _type;
}
