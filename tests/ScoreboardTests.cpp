#include <catch2/catch_test_macros.hpp>

#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/PlayerList.h>
#include <romanian_whist/RoundType.h>
#include <romanian_whist/Seat.h>
#include <romanian_whist/Scoreboard.h>
#include <romanian_whist/strategies/FirstCardStrategy.h>

#include <stdexcept>

using namespace romanian_whist;

namespace
{
std::unique_ptr<IMoveProvider> dummyProvider()
{
    return std::make_unique<AiMoveProvider>(std::make_unique<FirstCardStrategy>());
}

PlayerList buildPlayers(unsigned int count)
{
    PlayerList players;
    for(unsigned int i = 0 ; i < count ; i++)
        players.addPlayer("P" + std::to_string(i), dummyProvider());
    return players;
}

// Tricks won are counted off the tricks a round has stored, so a scoring test
// has to award real ones. Only the winner is read, so the tricks need no cards
// in them.
void awardTricks(Round& round, Seat winner, unsigned int count)
{
    for(unsigned int i = 0 ; i < count ; i++)
    {
        Trick trick;
        trick.setWinner(winner);
        round.addTrick(trick);
    }
}
}

TEST_CASE("Scoreboard round scoring", "[scoreboard]")
{
    PlayerList players = buildPlayers(2);
    Scoreboard scoreboard;
    scoreboard.initialize(GameStructure::S_181, false, false, players);

    const Seat a{0};
    const Seat b{1};

    // Skip the two 1-trick rounds the 2-player schedule opens with: both seats
    // take a trick here, which only a round of at least two can hold.
    scoreboard.incrementCurrentRound();
    scoreboard.incrementCurrentRound();
    REQUIRE(scoreboard.getCurrentRound().getTrickCount() == 2);

    Round& round = scoreboard.getCurrentRound();
    round.setBet(a, 1);
    awardTricks(round, a, 1);   // exact hit
    round.setBet(b, 0);
    awardTricks(round, b, 1);   // miss by 1

    scoreboard.calculateScores(players);

    REQUIRE(players.at(0).getCurrentRoundScore() == 5 + 1);
    REQUIRE(players.at(1).getCurrentRoundScore() == -1);
}

TEST_CASE("Scoreboard streaks", "[scoreboard]")
{
    PlayerList players = buildPlayers(2);
    Scoreboard scoreboard;
    scoreboard.initialize(GameStructure::S_181, false, false, players);

    const Seat a{0};
    const Seat b{1};

    SECTION("a 1-trick round neither breaks nor extends a streak")
    {
        REQUIRE(scoreboard.getCurrentRound().getTrickCount() == 1);

        Round& round = scoreboard.getCurrentRound();
        round.setBet(a, 0);
        round.setBet(b, 1);
        awardTricks(round, b, 1);   // a takes nothing: an exact hit on 0

        scoreboard.calculateScores(players);

        REQUIRE(players.at(0).getCurrentRoundScore() == 5);
        REQUIRE(players.at(0).getConsecutiveWins() == 0);
        REQUIRE(players.at(0).getConsecutiveLosses() == 0);
    }

    SECTION("five consecutive exact hits award +10 on the fifth")
    {
        // Skip the two 1-trick rounds this schedule opens with (2 players),
        // landing on the "2..7" ascending block, which every player count
        // has exactly six of.
        scoreboard.incrementCurrentRound();
        scoreboard.incrementCurrentRound();
        REQUIRE(scoreboard.getCurrentRound().getTrickCount() == 2);

        int previousTotal = players.at(0).getTotalScore();

        for(int i = 0 ; i < 5 ; i++)
        {
            Round& round = scoreboard.getCurrentRound();
            round.setBet(a, 0);
            round.setBet(b, 2);
            awardTricks(round, b, 2);   // a takes nothing: an exact hit every round

            scoreboard.calculateScores(players);
            scoreboard.commitRoundScores(players);

            const int total = players.at(0).getTotalScore();
            const int delta = total - previousTotal;

            REQUIRE(delta == (i < 4 ? 5 : 15));   // 5 + bid(0), +10 bonus on the 5th

            previousTotal = total;
            scoreboard.incrementCurrentRound();
        }

        REQUIRE(players.at(0).getConsecutiveWins() == 5);
    }

    SECTION("five consecutive misses subtract 10 on the fifth")
    {
        scoreboard.incrementCurrentRound();
        scoreboard.incrementCurrentRound();
        REQUIRE(scoreboard.getCurrentRound().getTrickCount() == 2);

        int previousTotal = players.at(0).getTotalScore();

        for(int i = 0 ; i < 5 ; i++)
        {
            Round& round = scoreboard.getCurrentRound();
            round.setBet(a, 0);
            round.setBet(b, 1);
            awardTricks(round, a, 1);   // a bid 0 and took 1: a miss, every round
            awardTricks(round, b, 1);

            scoreboard.calculateScores(players);
            scoreboard.commitRoundScores(players);

            const int total = players.at(0).getTotalScore();
            const int delta = total - previousTotal;

            REQUIRE(delta == (i < 4 ? -1 : -11));   // -1, -10 penalty on the 5th

            previousTotal = total;
            scoreboard.incrementCurrentRound();
        }

        REQUIRE(players.at(0).getConsecutiveLosses() == 5);
    }
}

namespace
{
std::vector<unsigned int> trickCountSchedule(GameStructure structure, unsigned int playerCount,
                                             bool endWithForeheadAndHidden)
{
    PlayerList players = buildPlayers(playerCount);
    Scoreboard scoreboard;
    scoreboard.initialize(structure, endWithForeheadAndHidden, false, players);

    std::vector<unsigned int> counts;
    const unsigned int roundCount = scoreboard.getRoundCount();
    for(unsigned int i = 0 ; i < roundCount ; i++)
    {
        counts.push_back(scoreboard.getCurrentRound().getTrickCount());
        if(i + 1 < roundCount)
            scoreboard.incrementCurrentRound();
    }
    return counts;
}
}

TEST_CASE("Scoreboard schedule shape", "[scoreboard]")
{
    SECTION("S_181: 1-block, ascending, 8-block, descending, 1-block")
    {
        for(unsigned int n : { 2u, 3u, 5u })
        {
            const auto counts = trickCountSchedule(GameStructure::S_181, n, false);
            REQUIRE(counts.size() == 3 * n + 12);

            std::vector<unsigned int> expected(n, 1);
            for(unsigned int t = 2 ; t <= 7 ; t++)
                expected.push_back(t);
            expected.insert(expected.end(), n, 8);
            for(unsigned int t = 7 ; t >= 2 ; t--)
                expected.push_back(t);
            expected.insert(expected.end(), n, 1);

            REQUIRE(counts == expected);
        }
    }

    SECTION("S_818: 8-block, descending, 1-block, ascending, 8-block")
    {
        for(unsigned int n : { 2u, 3u, 5u })
        {
            const auto counts = trickCountSchedule(GameStructure::S_818, n, false);
            REQUIRE(counts.size() == 3 * n + 12);

            std::vector<unsigned int> expected(n, 8);
            for(unsigned int t = 7 ; t >= 2 ; t--)
                expected.push_back(t);
            expected.insert(expected.end(), n, 1);
            for(unsigned int t = 2 ; t <= 7 ; t++)
                expected.push_back(t);
            expected.insert(expected.end(), n, 8);

            REQUIRE(counts == expected);
        }
    }

    SECTION("endWithForeheadAndHidden appends exactly two more 1-trick rounds")
    {
        const auto withoutExtra = trickCountSchedule(GameStructure::S_181, 3u, false);
        const auto withExtra = trickCountSchedule(GameStructure::S_181, 3u, true);

        REQUIRE(withExtra.size() == withoutExtra.size() + 2);
        REQUIRE(withExtra[withExtra.size() - 1] == 1);
        REQUIRE(withExtra[withExtra.size() - 2] == 1);
    }
}

TEST_CASE("Scoreboard opening seat advances by one seat per round", "[scoreboard]")
{
    const unsigned int n = 3;
    PlayerList players = buildPlayers(n);

    Scoreboard scoreboard;
    scoreboard.initialize(GameStructure::S_181, false, false, players);

    unsigned int expectedSeat = 0;
    const unsigned int roundCount = scoreboard.getRoundCount();
    for(unsigned int i = 0 ; i < roundCount ; i++)
    {
        REQUIRE(scoreboard.getCurrentRound().getOpenerSeat() == Seat{expectedSeat});
        expectedSeat = (expectedSeat + 1) % n;
        if(i + 1 < roundCount)
            scoreboard.incrementCurrentRound();
    }
}

TEST_CASE("Scoreboard all1GamesAreForehead", "[scoreboard]")
{
    const unsigned int n = 3;
    PlayerList players = buildPlayers(n);
    Scoreboard scoreboard;
    scoreboard.initialize(GameStructure::S_181, true, true, players);

    const unsigned int roundCount = scoreboard.getRoundCount();
    for(unsigned int i = 0 ; i < roundCount ; i++)
    {
        const Round& round = scoreboard.getCurrentRound();

        if(round.getTrickCount() == 1)
        {
            if(i >= roundCount - 2)
            {
                // The two appended rounds keep their own explicit type
                // regardless of all1GamesAreForehead.
                REQUIRE((round.getRoundType() == RoundType::Forehead ||
                         round.getRoundType() == RoundType::Hidden));
            }
            else
            {
                REQUIRE(round.getRoundType() == RoundType::Forehead);
            }
        }

        if(i + 1 < roundCount)
            scoreboard.incrementCurrentRound();
    }
}

TEST_CASE("Round::addTrick requires a winner that is at the table", "[round]")
{
    Round round(1, Seat{0}, 3);

    Trick unwon;

    // The scoring path reads nothing but the winner, so an unwon trick would
    // otherwise be stored and then counted for nobody.
    REQUIRE_THROWS_AS(round.addTrick(unwon), std::logic_error);

    Trick offTable;
    offTable.setWinner(Seat{3});
    REQUIRE_THROWS_AS(round.addTrick(offTable), std::out_of_range);

    REQUIRE(round.getPlayedTrickCount() == 0);

    Trick won;
    won.setWinner(Seat{2});
    round.addTrick(won);

    REQUIRE(round.getPlayedTrickCount() == 1);
    REQUIRE(round.getTricksWon(Seat{2}) == 1);
}
