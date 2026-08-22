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

void Hand::setWinner(PlayerList::iterator player)
{
    winner = player;
}

PlayerList::iterator Hand::getWinner()
{
    return winner;
}
