#ifndef AI_MOVE_PROVIDER_H
#define AI_MOVE_PROVIDER_H

#include <romanian_whist/IMoveProvider.h>
#include <romanian_whist/strategies/IStrategy.h>
#include <memory>

namespace romanian_whist
{
class AiMoveProvider : public IMoveProvider
{
private:
    std::unique_ptr<IStrategy> strategy;

public:
    AiMoveProvider(std::unique_ptr<IStrategy> _strategy);
    unsigned int makeBet(const BetContext& context) override;
    // The one place a value-choosing strategy meets the index-returning
    // provider boundary: it finds what the strategy picked in the hand and
    // returns that position. Every strategy stays in cards; the translation
    // happens here, once.
    std::optional<std::size_t> playCard(const PlayContext& context) override;
};

} // namespace romanian_whist

#endif
