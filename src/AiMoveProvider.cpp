#include <romanian_whist/AiMoveProvider.h>

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace romanian_whist
{
AiMoveProvider::AiMoveProvider(std::unique_ptr<IStrategy> _strategy) : strategy(std::move(_strategy))
{}

unsigned int AiMoveProvider::makeBet(const BetContext &context)
{
    return strategy->getBestBet(context);
}

std::optional<std::size_t> AiMoveProvider::playCard(const PlayContext &context)
{
    const std::optional<Card> chosen = strategy->getBestChoice(context);

    if(!chosen)
        return std::nullopt;

    // Cards are unique within a deck, so equality is identity here and the
    // first match is the only match. A miss means the strategy returned a card
    // that was never in the hand, which is a bug in the strategy rather than an
    // illegal move - so it says so, rather than surfacing as a mysterious
    // out-of-range index one frame later.
    const auto it = std::find(context.hand.begin(), context.hand.end(), *chosen);

    if(it == context.hand.end())
        throw std::logic_error("AiMoveProvider: the strategy chose " + chosen->toString()
                               + ", which is not in the hand");

    return static_cast<std::size_t>(std::distance(context.hand.begin(), it));
}

} // namespace romanian_whist
