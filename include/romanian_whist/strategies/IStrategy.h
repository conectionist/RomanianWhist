#ifndef ISTRATEGY_H
#define ISTRATEGY_H 

#include <romanian_whist/BetContext.h>
#include <romanian_whist/Card.h>
#include <romanian_whist/CardValidator.h>
#include <romanian_whist/PlayContext.h>

#include <optional>

#include <vector>

namespace romanian_whist
{
class IStrategy
{
protected:
    CardValidator cardValidator;
    
public:
    virtual ~IStrategy() = default;
    virtual unsigned int getBestBet(const BetContext& context) = 0;
    // The card to play, or empty when there is no legal play at all.
    // Strategies reason in cards; turning that into the index the engine
    // wants is AiMoveProvider's job, and only its job.
    virtual std::optional<Card> getBestChoice(const PlayContext& context) = 0;
};

} // namespace romanian_whist

#endif
