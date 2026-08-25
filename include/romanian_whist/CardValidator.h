#ifndef CARD_VALIDATOR_H
#define CARD_VALIDATOR_H

#include <romanian_whist/Card.h>

#include <vector>

namespace romanian_whist
{
class CardValidator
{
public:
    std::vector<Card*> getLegalCards(const std::vector<Card*>& hand, Card* trump, const Suit* leadSuit) const;

private:
    bool hasSuit(const std::vector<Card*>& hand, Suit suit) const;
};

} // namespace romanian_whist

#endif
