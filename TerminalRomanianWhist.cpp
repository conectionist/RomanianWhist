#include "TerminalRomanianWhist.h"

#include <iostream>

#include "Player.h"

using std::cin, std::cout, std::endl;

void TerminalRomanianWhist::startGame()
{
    //initialize();
    initializeTest();

    loop();
}

void TerminalRomanianWhist::initialize()
{
    int playerCount;
    cout << "Enter the number of players (2-6):" << endl;
    cin >> playerCount;

    vector<string> playerNames(playerCount);
    cout << "Enter the names of the " << playerCount << " players:" << endl;
    for(int i = 0 ; i < playerCount ; i++)
        cin >> playerNames[i];

    int gameStructure;
    cout << "Choose the game structure:" << endl;
    cout << "1) 1-8-1" << endl;
    cout << "2) 8-1-8" << endl;
    cin >> gameStructure;

    int endWithForeheadAndHidden;
    cout << "Should the game end with forehead and hidden hands?" << endl;
    cout << "1) Yes" << endl;
    cout << "2) No" << endl;
    cin >> endWithForeheadAndHidden;

    int all1GamesAreForehead;
    cout << "Should all 1-games be forehead?" << endl;
    cout << "1) Of course not!" << endl;
    cout << "2) Sure!" << endl;
    cin >> all1GamesAreForehead;

    game.createPlayers(playerNames);

    game.initializeScoreboard(gameStructure == 0 ? GameStructure::S_181 : GameStructure::S_818,
                              endWithForeheadAndHidden == 1,
                              all1GamesAreForehead == 1);

    game.initializeDeck(playerCount);
}

void TerminalRomanianWhist::loop()
{
    game.setStatus(GameStatus::InProgress);

    while(game.isInProgress())
    {
        /*
            shuffle deck
            deal cards
            show trump
            take bets from each player, starting with the current first
            wait for first player to put card
            wait for second player to put card
            ...
            wait for last player to put card
            decide who won the hand
            continue until players have no more cards
            display scoreboard

        */

        game.shuffleDeck();
        game.dealCards();

        auto* trump = game.getCurrentTrumpCard();
        if(trump)
            cout << "The trump card is " << trump->toString() << endl;
        else
            cout << "Game of 8. No trump card!" << endl;

        auto currentPlayer = game.getFirstPlayerOfTheRound();

        for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
        {
            unsigned int bet = currentPlayer->getBet();
            game.placeBet(currentPlayer, bet);
            currentPlayer = game.getNextPlayer(currentPlayer);
        }

        /*
            get card from 1st player
            set the down suit of the current round
            get cards for the rest of the players
            add each had to the round's hands vector
            decide who won based on the round's hand vector and set him/her as the new first player            
        */
        for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
        {
            
        }
    }
}

void TerminalRomanianWhist::initializeTest()
{
    int playerCount = 4;
    vector<string> playerNames = {"Danutz", "Mihai", "Aditz", "Fane"};
    
    game.createPlayers(playerNames);

    game.initializeScoreboard(GameStructure::S_181,
                              true,
                              false);

    game.initializeDeck(playerCount);
}
