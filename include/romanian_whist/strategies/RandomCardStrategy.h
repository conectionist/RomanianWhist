#ifndef RANDOM_CARD_STRATEGY_H
#define RANDOM_CARD_STRATEGY_H

#include <romanian_whist/strategies/IStrategy.h>

#include <cstdint>
#include <random>

namespace romanian_whist
{
class RandomCardStrategy : public IStrategy
{
private:
    std::mt19937 generator;

public:
    RandomCardStrategy();
    explicit RandomCardStrategy(std::uint32_t seed);

    unsigned int getBestBet(const BetContext& context) override;
    std::optional<Card> getBestChoice(const PlayContext& context) override;
};

} // namespace romanian_whist

#endif
