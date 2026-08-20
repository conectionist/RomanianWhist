#include "Round.h"

void Round::addHand(const Hand &hand)
{
    hands.push_back(hand);
}

void Round::addResult(Player *player, int wonHands)
{
    results.emplace_back(player, wonHands);
}
