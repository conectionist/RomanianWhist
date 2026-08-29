#ifndef TRICK_H
#define TRICK_H

#include <romanian_whist/Card.h>
#include <romanian_whist/Seat.h>

#include <vector>

namespace romanian_whist
{
// A card and the seat that played it. Phase 4 of ENGINE_V4_PLAN.md turns the
// card into a Card by value; the seat stays as it is.
struct PlayedCard
{
    Seat seat;
    Card* card;
};

class Trick
{
private:
    std::vector<PlayedCard> playedCards;
    Suit leadSuit;
    bool leadSuitSet;
    Seat winner;
    bool winnerSet;

public:
    Trick();

    void addPlayedCard(Seat seat, Card* card);

    // In the order the cards were played, so the first entry is the leader's.
    const std::vector<PlayedCard>& getPlayedCards() const;

    // The cards alone, in play order. PlayContext::playedCards is a flat list
    // of cards - no strategy asks who played what - so this is what gets handed
    // to a move provider. Built fresh each call, against a hand of at most
    // eight cards.
    std::vector<Card*> cardsInPlayOrder() const;

    void setLeadSuit(Suit suit);

    // Only meaningful once hasLeadSuit() is true: a trick with no cards played
    // yet has no lead suit, and the value returned then is the placeholder the
    // constructor installed.
    const Suit& getLeadSuit() const;
    bool hasLeadSuit() const;

    void setWinner(Seat seat);

    // Likewise, only meaningful once hasWinner() is true. Before setWinner()
    // the value returned is the placeholder the constructor installed, which
    // names seat 0 without meaning it.
    Seat getWinner() const;
    bool hasWinner() const;
};

} // namespace romanian_whist

#endif
