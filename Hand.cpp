#include "Hand.h"

void Hand::addPlayedCard(Card *card)
{
    playedCards.push_back(card);
}

vector<Card *> Hand::getPlayedCards()
{
    return playedCards;
}

void Hand::setDownSuit(Suit suit)
{
    downSuit = suit;
}

Suit Hand::getDownSuit()
{
    return downSuit;
}

void Hand::setWinner(Player *player)
{
    winner = player;
}

Player *Hand::getWinner()
{
    return winner;
}
