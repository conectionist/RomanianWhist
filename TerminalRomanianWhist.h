#ifndef TERMINAL_ROMANIAN_WHIST_H
#define TERMINAL_ROMANIAN_WHIST_H

#include "GameEngine.h"

class TerminalRomanianWhist
{
private:
    GameEngine game;

public:
    void startGame();

private:
    void initialize();
    void initializeTest();
    void loop();
    void playCurrentRoundTricks();
    void displayScoreboard();
    void displayFinalResults();



};

#endif
