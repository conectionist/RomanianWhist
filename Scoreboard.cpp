#include "Scoreboard.h"

#include <utility>
#include <algorithm>
#include <vector>

Scoreboard::Scoreboard() : currentRound(0)
{
}

void Scoreboard::initialize(const GameStructure &structure, 
                            bool endWithForeheadAndHidden, 
                            bool all1GamesAreForehead, 
                            PlayerList &players)
{
    auto currentPlayer = players.first();

    vector<unsigned int> gameNumbers;

    // TODO: 
    // for the moment we'll only consider the standard structure (1-8-1)
    // we'll implement 8-1-8 later
    if(true/*structure == GameStructure::S_181*/)
    {
        for(unsigned int i = 0 ; i < players.size() ; i++)
            gameNumbers.push_back(1);

        for(unsigned int i = 2 ; i <= 7 ; i++)
            gameNumbers.push_back(i);

        for(unsigned int i = 0 ; i < players.size() ; i++)
            gameNumbers.push_back(8);

        for(unsigned int i = 7 ; i >= 2 ; i--)
            gameNumbers.push_back(i);

        for(unsigned int i = 0 ; i < players.size() ; i++)
            gameNumbers.push_back(1);

        for(int i : gameNumbers)
        {
            Round r(i, currentPlayer);

            if(i == 1 && all1GamesAreForehead)
                r.setRoundType(RoundType::Forehead);  

            addRound(std::move(r));

            currentPlayer = players.next(currentPlayer);
        }
    }

    if(endWithForeheadAndHidden)
    {
        vector<RoundType> roundTypes = { RoundType::Forehead, RoundType::Hidden };

        for(auto type : roundTypes)
        {
            Round r(1, currentPlayer, type);

            addRound(std::move(r));

            currentPlayer = players.next(currentPlayer);
        }
    }
}

void Scoreboard::addRound(Round &&round)
{
    rounds.push_back(std::move(round));
}

void Scoreboard::incrementCurrentRound()
{
    currentRound++;
}

Round &Scoreboard::getCurrentRound()
{
    return rounds[currentRound];
}

unsigned int Scoreboard::getRoundCount() const
{
    return rounds.size();
}

unsigned int Scoreboard::getCurrentRoundIndex() const
{
    return currentRound;
}

void Scoreboard::calculateScores(PlayerList& players)
{
    Round& currentRound = getCurrentRound();
    
    for(auto& player : players)
    {
        const string& playerName = player.getName();
        
        // Get the bet and actual result for this player
        unsigned int bid = currentRound.getBet(playerName);
        unsigned int actual = currentRound.getActual(playerName);
        
        int roundScore = calculateRoundScore(bid, actual);
        player.addToScore(roundScore);
        
        // Update streak counters
        if(shouldCountForStreaks(currentRound))
        {
            if(bid == actual)
            {
                player.incrementConsecutiveWins();
                player.resetConsecutiveLosses();
                
                // Check for 5 consecutive wins
                if(player.getConsecutiveWins() == 5)
                {
                    player.addToScore(10); // Bonus for 5 consecutive wins
                }
            }
            else
            {
                player.incrementConsecutiveLosses();
                player.resetConsecutiveWins();
                
                // Check for 5 consecutive losses
                if(player.getConsecutiveLosses() == 5)
                {
                    player.addToScore(-10); // Penalty for 5 consecutive losses
                }
            }
        }
    }
}

void Scoreboard::commitRoundScores(PlayerList& players)
{
    // Commit round scores to total scores
    for(auto& player : players)
    {
        player.resetCurrentRoundScore();
    }
}

int Scoreboard::calculateRoundScore(unsigned int bid, unsigned int actual) const
{
    if(bid == actual)
    {
        return 5 + static_cast<int>(bid);
    }
    else
    {
        return -static_cast<int>(std::abs(static_cast<int>(bid) - static_cast<int>(actual)));
    }
}

bool Scoreboard::shouldCountForStreaks(const Round& round) const
{
    // 1-card rounds don't count for streaks
    return round.getTrickCount() != 1;
}

vector<pair<string, int>> Scoreboard::getPlayerScores(const PlayerList& players) const
{
    vector<pair<string, int>> playerScores;
    
    for(const auto& player : players)
    {
        playerScores.emplace_back(player.getName(), player.getTotalScore());
    }
    
    // Sort by total score (descending)
    std::sort(playerScores.begin(), playerScores.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return playerScores;
}
