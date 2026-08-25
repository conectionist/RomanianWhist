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
    unsigned int getRoundCount() const;
    unsigned int getCurrentRoundIndex() const;
    
    void calculateScores(PlayerList& players);
    void commitRoundScores(PlayerList& players);
    
    // Data access methods for display purposes
    std::vector<std::pair<std::string, int>> getPlayerScores(const PlayerList& players) const;

private:
    void addRound(Round&& round);
    int calculateRoundScore(unsigned int bid, unsigned int actual) const;
    bool shouldCountForStreaks(const Round& round) const;
};

} // namespace romanian_whist

#endif
