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
    unsigned int makeBet(const std::vector<Card*>& hand, Card* trump, bool isFirstPlayer) override;
    Card* playCard(const std::vector<Card*>& hand, Card* trump, const Suit* leadSuit) override;
};

} // namespace romanian_whist

#endif
