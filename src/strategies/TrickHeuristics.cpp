#include <romanian_whist/strategies/TrickHeuristics.h>

#include <romanian_whist/CardValidator.h>

#include <algorithm>

namespace romanian_whist::heuristics
{
namespace
{
// Every legal card that cannot take the trick as it currently stands.
std::vector<Card*> safeCards(const std::vector<Card*>& legalCards,
                             const Card& currentBest,
                             Suit leadSuit,
                             const Card* trump)
{
    std::vector<Card*> safe;

    for(Card* card : legalCards)
    {
        if(!CardValidator::beats(*card, currentBest, leadSuit, trump))
            safe.push_back(card);
    }

    return safe;
}

// Every legal card that would take the trick as it currently stands.
std::vector<Card*> winningCards(const std::vector<Card*>& legalCards,
                                const Card& currentBest,
                                Suit leadSuit,
                                const Card* trump)
{
    std::vector<Card*> winners;

    for(Card* card : legalCards)
    {
        if(CardValidator::beats(*card, currentBest, leadSuit, trump))
            winners.push_back(card);
    }

    return winners;
}

} // namespace

bool isMoreDangerous(const Card& a, const Card& b, const Card* trump)
{
    const bool aIsTrump = trump && a.suit == trump->suit;
    const bool bIsTrump = trump && b.suit == trump->suit;

    // A trump takes tricks its rank alone would never justify, so it outranks
    // every plain card here however small it is.
    if(aIsTrump != bIsTrump)
        return aIsTrump;

    return static_cast<int>(a.rank) > static_cast<int>(b.rank);
}

Card* mostDangerous(const std::vector<Card*>& cards, const Card* trump)
{
    if(cards.empty())
        return nullptr;

    return *std::max_element(cards.begin(), cards.end(),
                             [trump](const Card* a, const Card* b)
                             { return isMoreDangerous(*b, *a, trump); });
}

Card* leastDangerous(const std::vector<Card*>& cards, const Card* trump)
{
    if(cards.empty())
        return nullptr;

    return *std::min_element(cards.begin(), cards.end(),
                             [trump](const Card* a, const Card* b)
                             { return isMoreDangerous(*b, *a, trump); });
}

Card* chooseDuckingCard(const PlayContext& context, const std::vector<Card*>& legalCards)
{
    if(legalCards.empty())
        return nullptr;

    // Leading: no card is safe, so put down the one that would hurt least to
    // keep and hope somebody covers it.
    if(context.playedCards.empty() || context.leadSuit == nullptr)
        return leastDangerous(legalCards, context.trump);

    const Card* currentBest = CardValidator::getWinningCard(context.playedCards,
                                                            *context.leadSuit,
                                                            context.trump);

    if(currentBest == nullptr)
        return leastDangerous(legalCards, context.trump);

    const std::vector<Card*> safe = safeCards(legalCards, *currentBest, *context.leadSuit, context.trump);

    // The good case, and the whole point of the strategy: shed the biggest card
    // that still cannot win. A queen on the table with a jack and a nine in
    // hand sheds the jack; a ten on the table means the jack would win, so the
    // nine goes instead.
    if(!safe.empty())
        return mostDangerous(safe, context.trump);

    // Cornered into taking the trick. Take it with the least of the hand, which
    // both keeps the cheap cards for later and leaves the door open for someone
    // still to play to take it off us.
    return leastDangerous(legalCards, context.trump);
}

Card* chooseWinningCard(const PlayContext& context, const std::vector<Card*>& legalCards)
{
    if(legalCards.empty())
        return nullptr;

    // Leading: nothing beats holding the highest card out, so lead it.
    if(context.playedCards.empty() || context.leadSuit == nullptr)
        return mostDangerous(legalCards, context.trump);

    const Card* currentBest = CardValidator::getWinningCard(context.playedCards,
                                                            *context.leadSuit,
                                                            context.trump);

    if(currentBest == nullptr)
        return mostDangerous(legalCards, context.trump);

    const std::vector<Card*> winners = winningCards(legalCards, *currentBest, *context.leadSuit, context.trump);

    // Win it, but spend as little as the trick costs.
    if(!winners.empty())
        return leastDangerous(winners, context.trump);

    // This trick is already gone. Throw the cheapest card and keep the ones
    // that can still win a later one.
    return leastDangerous(legalCards, context.trump);
}

unsigned int countLikelyWinners(const std::vector<Card*>& hand, const Card* trump)
{
    unsigned int winners = 0;
    unsigned int trumpLength = 0;

    for(const Card* card : hand)
    {
        const bool isTrump = trump && card->suit == trump->suit;

        if(isTrump)
        {
            trumpLength++;

            // Only the top of the trump suit is near certain; the rest of it is
            // counted below, by length.
            if(card->rank == Rank::King || card->rank == Rank::Ace)
                winners++;
        }
        else if(card->rank == Rank::Ace)
        {
            // Can still be trumped, but a low-risk player would rather over-bid
            // an ace than be surprised by it.
            winners++;
        }
    }

    // Hold enough of the trump suit and the last of it takes tricks by itself,
    // once everyone else has run out. The first two are the ones already
    // counted above or spent drawing trumps out, so only the excess is new.
    if(trumpLength > 2)
        winners += trumpLength - 2;

    return std::min(winners, static_cast<unsigned int>(hand.size()));
}

} // namespace romanian_whist::heuristics
