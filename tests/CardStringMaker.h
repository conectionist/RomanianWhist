#ifndef CARD_STRING_MAKER_H
#define CARD_STRING_MAKER_H

#include <romanian_whist/Card.h>

#include <catch2/catch_tostring.hpp>

#include <string>

// Cards are compared by value all over the suite now, and without this every
// mismatch prints as "{ {?}, {?} }" - which says two hands differ but not how.
//
// This lives in its own header rather than in GameHarness.h because the two
// files with the heaviest by-value Card assertions - CardValidatorTests.cpp and
// TrickTests.cpp - do not use the harness at all, and so were still printing
// "{?}". It also has to be VISIBLE in every translation unit that compares
// Cards: a TU that instantiates the primary template while another explicitly
// specialises it is ill-formed with no diagnostic required. Include this at the
// top of every test file, and the two properties hold together.
namespace Catch
{
template<>
struct StringMaker<romanian_whist::Card>
{
    static std::string convert(const romanian_whist::Card& card)
    {
        return card.toString();
    }
};
}

#endif
