// The quick start from README.md, compiled.
//
// KEEP IN SYNC WITH README.md, "## Quick start". The body of TEST_CASE below is
// that snippet verbatim, minus `int main()` around it - if you change one,
// change the other. A published snippet that does not compile is worse than no
// snippet, and the v3 README is the proof: every sample in it named an API that
// had been private or deleted for a year, and nothing ever said so.
//
// The assertions are deliberately thin. What is being tested is that the code a
// reader copies still builds and still plays a game to the end; the rules it
// plays by are covered by the rest of the suite.

#include <catch2/catch_test_macros.hpp>

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>
#include <romanian_whist/strategies/RandomCardStrategy.h>

#include <iostream>
#include <memory>
#include <sstream>

using namespace romanian_whist;

namespace
{
// ---- README snippet, part 1 ----
class Commentator : public IGameObserver
{
public:
    void onTrickWon(const GameEngine& engine, Seat winner, unsigned int trickNumber) override
    {
        std::cout << "Trick " << trickNumber << " to "
                  << engine.getPlayers().at(winner.index).getName() << '\n';
    }

    void onGameOver(const GameEngine& engine) override
    {
        for(const Standing& standing : engine.getStandings())
            std::cout << standing.name << ": " << standing.score << '\n';
    }
};
// ---- end part 1 ----

// Swaps std::cout's buffer for the duration of the test, so the snippet can be
// copied verbatim - prints and all - without 26 rounds of commentary landing in
// the test output.
class CapturedOutput
{
private:
    std::ostringstream captured;
    std::streambuf* previous;

public:
    CapturedOutput() : previous(std::cout.rdbuf(captured.rdbuf())) {}
    ~CapturedOutput() { std::cout.rdbuf(previous); }

    std::string str() const { return captured.str(); }
};

std::unique_ptr<IMoveProvider> bot(std::uint32_t seed)
{
    return std::make_unique<AiMoveProvider>(std::make_unique<RandomCardStrategy>(seed));
}
} // namespace

TEST_CASE("the README quick start compiles and plays a game to the end",
          "[readme][quickstart]")
{
    const CapturedOutput output;

    // ---- README snippet, part 2 ----
    // The observer is declared first, so it is destroyed last: addObserver()
    // takes a raw pointer and the engine must not outlive what it points at.
    Commentator commentator;
    GameEngine game;

    game.addObserver(&commentator);

    GameSetup setup;
    setup.seats.push_back({"Ana", bot(1)});
    setup.seats.push_back({"Bogdan", bot(2)});
    setup.seats.push_back({"Carmen", bot(3)});
    setup.structure = GameStructure::S_181;
    setup.shuffleSeed = 42;

    game.start(std::move(setup));
    game.run();
    // ---- end part 2 ----

    CHECK(game.getStatus() == GameStatus::Finished);
    CHECK(game.getStandings().size() == 3);
    CHECK_FALSE(output.str().empty());
}
