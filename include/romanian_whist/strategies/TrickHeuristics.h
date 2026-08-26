#ifndef TRICK_HEURISTICS_H
#define TRICK_HEURISTICS_H

#include <romanian_whist/Card.h>
#include <romanian_whist/PlayContext.h>

#include <vector>

namespace romanian_whist::heuristics
{
// The judgements a player makes when it would rather not take tricks, kept out
// of the strategies themselves so that more than one can be built on them
// without either inheriting from the other.

// Orders cards by how much trouble they are to a player trying not to win: any
// trump is worse than any plain card, and rank decides within that. It is a
// strict weak ordering, so it can be handed to the standard algorithms.
bool isMoreDangerous(const Card& a, const Card& b, const Card* trump);

// The extremes of that ordering. Both return null for an empty list.
Card* mostDangerous(const std::vector<Card*>& cards, const Card* trump);
Card* leastDangerous(const std::vector<Card*>& cards, const Card* trump);

// The play that best avoids taking the trick.
//
// Leading, nothing is safe - whatever goes down might hold - so it leads its
// least dangerous card. Following, a card that does not beat the one currently
// winning cannot take the trick no matter who plays after us, since later
// players only push the winner higher. That makes "safe" exact rather than a
// guess, so it dumps the most dangerous card that is still safe, and only when
// every legal card would win does it settle for winning as cheaply as it can.
Card* chooseDuckingCard(const PlayContext& context, const std::vector<Card*>& legalCards);

// The mirror image, for a player that still owes tricks on its bid: the
// cheapest card that takes the trick, or - when none of them can - the least
// dangerous card, holding the good ones back for a trick still worth having.
Card* chooseWinningCard(const PlayContext& context, const std::vector<Card*>& legalCards);

// A rough count of the tricks a hand will take whether its holder wants them or
// not, which for a low-risk player is the same thing as the bid it should make.
// Deliberately crude: aces, the top trumps, and the length of a long trump
// holding that will still be out when everyone else has run dry.
unsigned int countLikelyWinners(const std::vector<Card*>& hand, const Card* trump);

} // namespace romanian_whist::heuristics

#endif
