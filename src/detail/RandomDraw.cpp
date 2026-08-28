#include <romanian_whist/detail/RandomDraw.h>

namespace romanian_whist::detail
{
unsigned int uniformIndex(std::mt19937& generator, unsigned int exclusiveUpperBound)
{
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
