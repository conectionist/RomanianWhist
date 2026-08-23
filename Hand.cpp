#include "Hand.h"

void Hand::addPlayedCard(Card *card)
{
    playedCards.push_back(card);
}

vector<Card *> Hand::getPlayedCards() const
{
    return playedCards;
}

void Hand::setLeadSuit(Suit suit)
{
    leadSuit = suit;
}

const Suit& Hand::getLeadSuit() const
{
    return leadSuit;
}

void Hand::setWinner(PlayerList::iterator player)
{
    winner = player;
}

PlayerList::iterator Hand::getWinner()
{
    return winner;
}
