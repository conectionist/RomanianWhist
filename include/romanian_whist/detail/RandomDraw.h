#ifndef RANDOM_DRAW_H
#define RANDOM_DRAW_H

#include <random>

namespace romanian_whist::detail
{
// Uniform draw in [0, exclusiveUpperBound) by rejection sampling against
// generator(). std::uniform_int_distribution's draw count is
// implementation-defined and differs between libstdc++, MSVC STL and
// libc++, which would make a seeded stream non-reproducible across
// platforms; this does not. Precondition: exclusiveUpperBound > 0.
unsigned int uniformIndex(std::mt19937& generator, unsigned int exclusiveUpperBound);

} // namespace romanian_whist::detail

#endif
