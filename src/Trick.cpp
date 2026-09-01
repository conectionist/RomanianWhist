#include <romanian_whist/Trick.h>

namespace romanian_whist
{
// leadSuit and winner both need *some* value: reading an uninitialised enum is
// undefined even when callers are disciplined about checking hasLeadSuit()
// first, and a seat that named nobody would be a lie either way. leadSuitSet
// and winnerSet are what tell a real value from these placeholders.
Trick::Trick() : leadSuit(Suit::Hearts),
                 leadSuitSet(false),
                 winner(0),
                 winnerSet(false)
{}

void Trick::addPlayedCard(Seat seat, Card card)
{
    playedCards.push_back({ seat, card });
}

const std::vector<PlayedCard>& Trick::getPlayedCards() const
{
    return playedCards;
}

std::vector<Card> Trick::cardsInPlayOrder() const
{
    std::vector<Card> cards;
    cards.reserve(playedCards.size());

    for(const PlayedCard& played : playedCards)
        cards.push_back(played.card);

    return cards;
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

void Trick::setWinner(Seat seat)
{
    winner = seat;
    winnerSet = true;
}

Seat Trick::getWinner() const
{
    return winner;
}

bool Trick::hasWinner() const
{
    return winnerSet;
}

} // namespace romanian_whist
