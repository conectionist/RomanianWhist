// The quick start from README.md, compiled - and checked against the README.
//
// The body of the first TEST_CASE below is that snippet verbatim, minus the
// includes and the `int main()` around it. The second TEST_CASE re-reads
// README.md and compares the two, so editing one without the other fails the
// suite rather than quietly shipping a stale snippet. A published snippet that
// does not compile is worse than no snippet, and the v3 README is the proof:
// every sample in it named an API that had been private or deleted for a year,
// and nothing ever said so.
//
// The assertions in the first case are deliberately thin. What is being tested
// is that the code a reader copies still builds and still plays a game to the
// end; the rules it plays by are covered by the rest of the suite.

#include <catch2/catch_test_macros.hpp>

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/IGameObserver.h>
#include <romanian_whist/strategies/RandomCardStrategy.h>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace romanian_whist;

namespace
{
// Swaps std::cout's buffer for the duration of the test, so the snippet can be
// copied verbatim - prints and all - without the game's commentary landing in
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

std::unique_ptr<IMoveProvider> bot(std::uint32_t seed)
{
    return std::make_unique<AiMoveProvider>(std::make_unique<RandomCardStrategy>(seed));
}
// ---- end part 1 ----
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

// ---------------------------------------------------------------------------
// Everything below reads the two files back off disk and compares them. It is
// what makes "keep these in sync" an invariant instead of a request.

namespace
{
std::vector<std::string> readLines(const char* path)
{
    std::ifstream file(path);

    if(!file)
        throw std::runtime_error(std::string("cannot open ") + path);

    std::vector<std::string> lines;
    std::string line;

    while(std::getline(file, line))
    {
        if(!line.empty() && line.back() == '\r')
            line.pop_back();

        lines.push_back(line);
    }

    return lines;
}

std::string trimmed(const std::string& line)
{
    const std::size_t first = line.find_first_not_of(" \t");

    if(first == std::string::npos)
        return "";

    const std::size_t last = line.find_last_not_of(" \t");
    return line.substr(first, last - first + 1);
}

std::size_t lineIndex(const std::vector<std::string>& lines, const std::string& wanted, std::size_t from = 0)
{
    for(std::size_t index = from; index < lines.size(); ++index)
        if(trimmed(lines[index]) == wanted)
            return index;

    throw std::runtime_error("no line \"" + wanted + "\"");
}

// Every #include of an engine header, in the order they appear.
std::vector<std::string> engineIncludes(const std::vector<std::string>& lines)
{
    std::vector<std::string> includes;

    for(const std::string& line : lines)
        if(trimmed(line).rfind("#include <romanian_whist/", 0) == 0)
            includes.push_back(trimmed(line));

    return includes;
}

std::string join(const std::vector<std::string>& lines, std::size_t from, std::size_t to)
{
    std::string joined;

    for(std::size_t index = from; index < to; ++index)
        joined += lines[index] + '\n';

    return joined;
}

// The first ```cpp block under a heading, fences excluded.
std::vector<std::string> fencedBlockUnder(const std::vector<std::string>& lines, const std::string& heading)
{
    std::size_t index = lineIndex(lines, "```cpp", lineIndex(lines, heading)) + 1;
    std::vector<std::string> block;

    for(; index < lines.size() && trimmed(lines[index]) != "```"; ++index)
        block.push_back(lines[index]);

    return block;
}

// The lines between the part markers, markers themselves excluded. Built by
// concatenation rather than written out, so that the markers this function
// looks for do not also appear in this function.
std::string markedRegion(const std::vector<std::string>& lines, const std::string& part)
{
    const std::size_t begin = lineIndex(lines, "// ---- README snippet, part " + part + " ----") + 1;
    const std::size_t end = lineIndex(lines, "// ---- end part " + part + " ----", begin);
    return join(lines, begin, end);
}

// Drops the trailing blank lines a region picks up from the blank line that
// separates it from whatever follows.
std::string withoutTrailingBlanks(std::string text)
{
    while(text.size() >= 2 && text[text.size() - 2] == '\n')
        text.pop_back();

    return text;
}
} // namespace

TEST_CASE("the compiled quick start is the one printed in the README", "[readme][quickstart]")
{
    const std::vector<std::string> readme = readLines(WHIST_README_PATH);
    const std::vector<std::string> test = readLines(WHIST_QUICKSTART_TEST_PATH);

    const std::vector<std::string> snippet = fencedBlockUnder(readme, "## Quick start");

    const std::size_t declarations = lineIndex(snippet, "class Commentator : public IGameObserver");
    const std::size_t main = lineIndex(snippet, "int main()", declarations);

    REQUIRE(trimmed(snippet.at(main + 1)) == "{");
    REQUIRE(trimmed(snippet.back()) == "}");

    // Part 1 is the declarations above main(); part 2 is main()'s body, brace
    // lines excluded at both ends.
    CHECK(withoutTrailingBlanks(join(snippet, declarations, main)) == markedRegion(test, "1"));
    CHECK(join(snippet, main + 2, snippet.size() - 1) == markedRegion(test, "2"));

    // The engine headers have to match exactly, in both directions. A test
    // including one the README does not is a snippet that will not compile for
    // the reader; a README listing one the test does not is a snippet nobody is
    // compiling as printed.
    CHECK(engineIncludes(snippet) == engineIncludes(test));
}
