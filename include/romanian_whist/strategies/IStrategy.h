#ifndef ISTRATEGY_H
#define ISTRATEGY_H 

#include <romanian_whist/BetContext.h>
#include <romanian_whist/Card.h>
#include <romanian_whist/CardValidator.h>
#include <romanian_whist/PlayContext.h>

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
    virtual Card* getBestChoice(const PlayContext& context) = 0;
};

} // namespace romanian_whist

#endif
