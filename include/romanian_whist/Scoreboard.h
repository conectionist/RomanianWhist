#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include <romanian_whist/Round.h>
#include <romanian_whist/PlayerList.h>

#include <vector>
#include <utility>

namespace romanian_whist
{
enum class GameStructure
{
    S_181,
    S_818
};

class Scoreboard
{
private:
    std::vector<Round> rounds;
    unsigned int currentRound;

public:
    Scoreboard();
    void initialize(const GameStructure& structure, 
                    bool endWithForeheadAndHidden, 
                    bool all1GamesAreForehead,
                    PlayerList& players);
    
    void incrementCurrentRound();
    Round& getCurrentRound();
    const Round& getCurrentRound() const;

    // Any round in the schedule. initialize() lays the whole schedule out up
    // front, so a round past getCurrentRoundIndex() exists but has not been
    // played yet - no bets, no tricks, no trump. Unguarded, like
    // getCurrentRound(): GameEngine::getRound() is the checked way in.
    const Round& getRound(unsigned int index) const;

    unsigned int getRoundCount() const;
    unsigned int getCurrentRoundIndex() const;
    
    void calculateScores(PlayerList& players);
    void commitRoundScores(PlayerList& players);

private:
    void addRound(Round&& round);
    int calculateRoundScore(unsigned int bid, unsigned int actual) const;
    bool shouldCountForStreaks(const Round& round) const;
};

} // namespace romanian_whist

#endif
