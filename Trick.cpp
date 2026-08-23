#include "Trick.h"

void Trick::addPlayedCard(Card *card)
{
    playedCards.push_back(card);
}

vector<Card *> Trick::getPlayedCards() const
{
    return playedCards;
}

void Trick::setLeadSuit(Suit suit)
{
    leadSuit = suit;
}

const Suit& Trick::getLeadSuit() const
{
    return leadSuit;
}

void Trick::setWinner(PlayerList::iterator player)
{
    winner = player;
}

PlayerList::iterator Trick::getWinner()
{
    return winner;
}
