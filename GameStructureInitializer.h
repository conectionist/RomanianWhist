#ifndef GAME_STRUCTURE_INITIALIZER_H
#define GAME_STRUCTURE_INITIALIZER_H

#include "Scoreboard.h"

enum class GameStructure
{
    S_181,
    S_818
};

class GameStructureInitializer
{
public:
    static void initialize(Scoreboard& scoreboard, 
                    const GameStructure& structure, 
                    bool endWithForeheadAndHidden, 
                    bool all1GamesAreForehead, 
                    const vector<Player>& players);

};

#endif
