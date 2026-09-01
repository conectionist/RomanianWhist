#ifndef CARD_STRING_MAKER_H
#define CARD_STRING_MAKER_H

#include <romanian_whist/Card.h>

// Half the by-value Card assertions in the suite are on an optional: trump,
// getWinningCard(), and everything TrickHeuristics returns. Catch2 prints those
// as "{?}" - even with the Card specialization below in scope - unless
// CATCH_CONFIG_ENABLE_OPTIONAL_STRINGMAKER is defined, so a failure reads
// "{?} == QS" and names only the half that was already right.
//
// It is set on the test target rather than here: the macro has to be seen
// before catch_tostring.hpp, and a test file that includes catch_test_macros.hpp
// first - as most of them do - has already pulled that in by the time it reaches
// this header. A compile definition cannot be defeated by include order.
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
