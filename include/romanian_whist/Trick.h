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
    PlayerList::iterator winner;

public:
    void addPlayedCard(Card* card);
    std::vector<Card*> getPlayedCards() const;

    void setLeadSuit(Suit suit);
    const Suit& getLeadSuit() const;

    void setWinner(PlayerList::iterator player);
    PlayerList::iterator getWinner();
};

} // namespace romanian_whist

#endif
