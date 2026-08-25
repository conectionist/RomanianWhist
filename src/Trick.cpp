#include <romanian_whist/Trick.h>

namespace romanian_whist
{
// leadSuit needs *some* value: reading an uninitialised enum is undefined even
// when callers are disciplined about checking hasLeadSuit() first. The winner
// iterator cannot be given a valid value here (there is no list to point into),
// so winnerSet guards it instead.
Trick::Trick() : leadSuit(Suit::Hearts),
                 leadSuitSet(false),
                 winnerSet(false)
{}

void Trick::addPlayedCard(Card *card)
{
    playedCards.push_back(card);
}

const std::vector<Card *>& Trick::getPlayedCards() const
{
    return playedCards;
}

void Trick::setLeadSuit(Suit suit)
{
    leadSuit = suit;
    leadSuitSet = true;
}

const Suit& Trick::getLeadSuit() const
{
    return leadSuit;
}

bool Trick::hasLeadSuit() const
{
    return leadSuitSet;
}

void Trick::setWinner(PlayerList::iterator player)
{
    winner = player;
    winnerSet = true;
}

PlayerList::iterator Trick::getWinner()
{
    return winner;
}

PlayerList::const_iterator Trick::getWinner() const
{
    return winner;
}

bool Trick::hasWinner() const
{
    return winnerSet;
}

} // namespace romanian_whist
