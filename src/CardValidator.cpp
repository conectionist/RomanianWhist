#include <romanian_whist/CardValidator.h>

namespace romanian_whist
{
std::vector<Card> CardValidator::getLegalCards(const std::vector<Card>& hand,
                                               std::optional<Card> trump,
                                               std::optional<Suit> leadSuit) const
{
    std::vector<Card> legalCards;

    if(hand.empty())
        return legalCards;

    if(!leadSuit)
        return hand;

    const bool mustFollowSuit = hasSuit(hand, *leadSuit);
    const bool mustPlayTrump = !mustFollowSuit && trump && hasSuit(hand, trump->suit);

    // Pushed in hand order, and that matters beyond tidiness: mostDangerous()
    // and leastDangerous() break ties by position, so reordering here would
    // quietly change which of two equally ranked cards a strategy plays.
    for(const Card& card : hand)
    {
        if(mustFollowSuit)
        {
            if(card.suit == *leadSuit)
                legalCards.push_back(card);
        }
        else if(mustPlayTrump)
        {
            if(card.suit == trump->suit)
                legalCards.push_back(card);
        }
        else
        {
            legalCards.push_back(card);
        }
    }

    return legalCards;
}

bool CardValidator::beats(const Card& candidate, const Card& currentBest,
                          Suit leadSuit, std::optional<Card> trump)
{
    const bool candidateIsTrump = trump && candidate.suit == trump->suit;
    const bool bestIsTrump = trump && currentBest.suit == trump->suit;

    if(candidateIsTrump || bestIsTrump)
    {
        if(candidateIsTrump != bestIsTrump)
            return candidateIsTrump;

        return static_cast<int>(candidate.rank) > static_cast<int>(currentBest.rank);
    }

    const bool candidateIsLead = candidate.suit == leadSuit;
    const bool bestIsLead = currentBest.suit == leadSuit;

    if(candidateIsLead != bestIsLead)
        return candidateIsLead;

    if(candidateIsLead)
        return static_cast<int>(candidate.rank) > static_cast<int>(currentBest.rank);

    // Two off-suit discards: neither can take the trick, so the one already
    // there stays ahead.
    return false;
}

std::optional<Card> CardValidator::getWinningCard(const std::vector<Card>& playedCards,
                                                  Suit leadSuit, std::optional<Card> trump)
{
    std::optional<Card> best;

    for(const Card& card : playedCards)
    {
        if(!best || beats(card, *best, leadSuit, trump))
            best = card;
    }

    return best;
}

bool CardValidator::hasSuit(const std::vector<Card>& hand, Suit suit) const
{
    for(const Card& card : hand)
    {
        if(card.suit == suit)
            return true;
    }

    return false;
}

} // namespace romanian_whist
