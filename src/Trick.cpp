#include <romanian_whist/Trick.h>

namespace romanian_whist
{
void Trick::addPlayedCard(Card *card)
{
    playedCards.push_back(card);
}

std::vector<Card *> Trick::getPlayedCards() const
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

} // namespace romanian_whist
