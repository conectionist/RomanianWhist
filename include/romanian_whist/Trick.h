#ifndef TRICK_H
#define TRICK_H

#include <romanian_whist/Card.h>
#include <romanian_whist/PlayerList.h>

#include <vector>

namespace romanian_whist
{
class Trick
{
private:
    std::vector<Card*> playedCards;
    Suit leadSuit;
    bool leadSuitSet;
    PlayerList::iterator winner;
    bool winnerSet;

public:
    Trick();

    void addPlayedCard(Card* card);
    const std::vector<Card*>& getPlayedCards() const;

    void setLeadSuit(Suit suit);

    // Only meaningful once hasLeadSuit() is true: a trick with no cards played
    // yet has no lead suit, and the value returned then is the placeholder the
    // constructor installed.
    const Suit& getLeadSuit() const;
    bool hasLeadSuit() const;

    void setWinner(PlayerList::iterator player);

    // Likewise, only meaningful once hasWinner() is true. Before setWinner() the
    // stored iterator is singular and must not be dereferenced or compared.
    PlayerList::iterator getWinner();
    PlayerList::const_iterator getWinner() const;
    bool hasWinner() const;
};

} // namespace romanian_whist

#endif
