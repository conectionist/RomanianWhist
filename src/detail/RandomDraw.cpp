#include <romanian_whist/detail/RandomDraw.h>

#include <stdexcept>

namespace romanian_whist::detail
{
unsigned int uniformIndex(std::mt19937& generator, unsigned int exclusiveUpperBound)
{
    // Not an assert: this still has to fail loudly in the release
    // configuration, where NDEBUG strips assert() and the alternative is a
    // silent division by zero below.
    if(exclusiveUpperBound == 0)
        throw std::invalid_argument("uniformIndex: exclusiveUpperBound must be > 0");

    using ResultType = std::mt19937::result_type;

    const ResultType range = exclusiveUpperBound;
    const ResultType limit = (std::mt19937::max() / range) * range;

    ResultType value;
    do
    {
        value = generator();
    } while(value >= limit);

    return static_cast<unsigned int>(value % range);
}

} // namespace romanian_whist::detail
