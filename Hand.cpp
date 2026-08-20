#include "Hand.h"

void Hand::addPlayedCard(Card *card)
{
    playedCards.push_back(card);
}

vector<Card *> Hand::getPlayedCards()
{
    return playedCards;
}
