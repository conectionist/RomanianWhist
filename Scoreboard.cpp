#include "Scoreboard.h"

#include <utility>

Scoreboard::Scoreboard() : currentRound(0)
{
}

void Scoreboard::initialize(const GameStructure &structure, 
                            bool endWithForeheadAndHidden, 
                            bool all1GamesAreForehead, 
                            const vector<Player> &players)
{
    unsigned int playerIndex = 0;

    auto incrementPlayerIndex = [&playerIndex, &players]()
    {
        if(playerIndex == players.size() - 1)
            playerIndex = 0;
        else
            playerIndex++;
    };

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
            Round r(i, &players[playerIndex]);

            if(i == 1 && all1GamesAreForehead)
                r.setRoundType(RoundType::Forehead);  

            addRound(std::move(r));

            incrementPlayerIndex();            
        }
    }

    if(endWithForeheadAndHidden)
    {
        vector<RoundType> roundTypes = { RoundType::Forehead, RoundType::Hidden };

        for(auto type : roundTypes)
        {
            Round r(1, &players[playerIndex], type);

            addRound(std::move(r));

            incrementPlayerIndex();
        }
    }
}

// void Scoreboard::addRound(const Round &round)
// {
//     rounds.push_back(round);
// }

void Scoreboard::addRound(Round &&round)
{
    rounds.push_back(std::move(round));
}

Round &Scoreboard::getRound(int i)
{
    return rounds[i];
}

void Scoreboard::incrementCurrentRound()
{
    currentRound++;
}

Round &Scoreboard::getCurrentRound()
{
    return getRound(currentRound);
}

vector<Round> Scoreboard::getAllRounds()
{
    return rounds;
}
