#ifndef CARD_VALIDATOR_H
#define CARD_VALIDATOR_H

#include <romanian_whist/Card.h>

#include <optional>
#include <vector>

namespace romanian_whist
{
class CardValidator
{
public:
    // Cards by value throughout: an empty `trump` is an 8-card round, and an
    // empty `leadSuit` means this player is leading and so is the one setting
    // it. Both used to be pointers whose null meant "none", which is what an
    // optional says outright.
    std::vector<Card> getLegalCards(const std::vector<Card>& hand,
                                    std::optional<Card> trump,
                                    std::optional<Suit> leadSuit) const;

    // The one ranking rule in the game: trump beats plain, otherwise the lead
    // suit beats an off-suit discard, and within a suit the higher rank wins.
    // Static because it is a pure function of its arguments - GameEngine calls
    // it without holding a validator, and every IStrategy gets it for free
    // through the one it inherits.
    static bool beats(const Card& candidate, const Card& currentBest,
                      Suit leadSuit, std::optional<Card> trump);

    // The card currently taking the trick, or empty for a trick with no cards
    // played yet. Copes with a partly played trick, so it also answers "who is
    // winning so far?".
    static std::optional<Card> getWinningCard(const std::vector<Card>& playedCards,
                                              Suit leadSuit, std::optional<Card> trump);

private:
    bool hasSuit(const std::vector<Card>& hand, Suit suit) const;
};

} // namespace romanian_whist

#endif
