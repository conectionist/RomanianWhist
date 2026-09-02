#include <romanian_whist/strategies/LowRiskStrategy.h>

#include <romanian_whist/strategies/TrickHeuristics.h>

namespace romanian_whist
{
unsigned int LowRiskStrategy::getBestBet(const BetContext &context)
{
    // Forehead and Hidden both mean this hand cannot be read to bid on - there
    // is nothing to count likely winners from, so bid nothing.
    const bool blind = context.roundType == RoundType::Forehead ||
                       context.roundType == RoundType::Hidden;

    unsigned int bet = blind ? 0u : heuristics::countLikelyWinners(context.hand, context.trump);

    if(context.forbiddenBet && bet == *context.forbiddenBet)
    {
        // Barred from the bid it wanted, it steps down rather than up: shedding
        // one trick it expected to win is something a hand full of ducking
        // chances can usually manage, whereas forcing an extra trick out of a
        // hand that has none is a coin flip. Stepping down from zero is not an
        // option, and a round always has at least one trick, so one is safe.
        bet = (bet == 0u) ? 1u : bet - 1u;
    }

    return bet;
}

std::optional<Card> LowRiskStrategy::getBestChoice(const PlayContext &context)
{
    const std::vector<Card> legalCards = cardValidator.getLegalCards(context.hand,
                                                                      context.trump,
                                                                      context.leadSuit);

    const bool owesTricks = context.tricksWon < context.bet;

    // Once the bid is met - or overshot - every further trick is a penalty, so
    // it spends the rest of the round getting rid of its dangerous cards.
    return owesTricks ? heuristics::chooseWinningCard(context, legalCards)
                      : heuristics::chooseDuckingCard(context, legalCards);
}

} // namespace romanian_whist
