#include "TerminalRomanianWhist.h"

#include <iostream>
#include <unordered_map>
#include <iomanip>
#include <algorithm>

#include "Hand.h"
#include "Player.h"
#include "ConsoleMoveProvider.h"

using std::cin, std::cout, std::endl, std::left, std::setw;

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

    for(const auto& name : playerNames)
        game.addPlayer(name, std::make_unique<ConsoleMoveProvider>());

    game.initializeScoreboard(gameStructure == 0 ? GameStructure::S_181 : GameStructure::S_818,
                              endWithForeheadAndHidden == 1,
                              all1GamesAreForehead == 1);

    game.initializeDeck(playerCount);
}

void TerminalRomanianWhist::initializeTest()
{
    int playerCount = 4;
    
    game.addPlayer("Danutz", std::make_unique<ConsoleMoveProvider>());
    game.addPlayer("Mihai", std::make_unique<ConsoleMoveProvider>());
    game.addPlayer("Aditz", std::make_unique<ConsoleMoveProvider>());
    game.addPlayer("Fane", std::make_unique<ConsoleMoveProvider>());
    
    game.initializeScoreboard(GameStructure::S_181,
                              true,
                              false);

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

        cout << "\n=========================\n";

        cout << "Joc de " << game.getCurrentRoundHandCount() << endl;

        auto* trump = game.getCurrentTrumpCard();
        if(trump)
            cout << "The trump card is " << trump->toString() << endl;
        else
            cout << "Game of 8. No trump card!" << endl;

        auto currentPlayer = game.getFirstPlayerOfTheRound();

        for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
        {
            bool isFirstPlayer = currentPlayer->getName() == game.getFirstPlayerOfTheRound()->getName();
            unsigned int bet = currentPlayer->getBet(game.getCurrentTrumpCard(), isFirstPlayer);
            game.placeBet(currentPlayer, bet);
            currentPlayer = game.getNextPlayer(currentPlayer);
        }

        playCurrentRoundTricks();
        
        // Calculate scores (but don't reset yet so we can display round scores)
        game.calculateScores();
        displayScoreboard();
        
        // Now commit the round scores to total scores
        game.commitRoundScores();
        
        game.completeCurrentRound();

        cout << "\n=========================\n";
    }
    
    // Display final results
    cout << "\n=========================\n";
    cout << "GAME OVER" << endl;
    cout << "=========================\n";
    displayFinalResults();
}

void TerminalRomanianWhist::playCurrentRoundTricks()
{
    const unsigned int trickCount = game.getCurrentRoundHandCount();
    
    // Track tricks won by each player
    std::unordered_map<std::string, unsigned int> tricksWon;
    
    // Initialize all players with 0 tricks won
    auto firstPlayer = game.getFirstPlayerOfTheRound();
    auto currentPlayer = firstPlayer;
    for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
    {
        tricksWon[currentPlayer->getName()] = 0;
        currentPlayer = game.getNextPlayer(currentPlayer);
    }

    for(unsigned int trickIndex = 0 ; trickIndex < trickCount ; trickIndex++)
    {
        Hand trick;
        currentPlayer = game.getFirstPlayerOfTheRound();

        cout << endl << "Trick " << (trickIndex + 1) << " of " << trickCount << endl;

        for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
        {
            Card* trump = game.getCurrentTrumpCard();
            const Suit* downSuit = trick.getPlayedCards().empty() ? nullptr : &trick.getDownSuit();
            Card* playedCard = currentPlayer->playCard(trump, downSuit);

            if(trick.getPlayedCards().empty())
                trick.setDownSuit(playedCard->suit);

            trick.addPlayedCard(playedCard);
            cout << currentPlayer->getName() << " plays " << playedCard->toString() << endl;

            currentPlayer = game.getNextPlayer(currentPlayer);
        }

        auto winner = game.determineTrickWinner(trick, game.getFirstPlayerOfTheRound());
        trick.setWinner(winner);
        game.addHandToCurrentRound(trick);
        game.setFirstPlayerOfTheRound(winner);

        // Track tricks won
        tricksWon[winner->getName()]++;
        
        cout << winner->getName() << " wins the trick." << endl;
    }
    
    // Set the results for each player
    currentPlayer = firstPlayer;
    for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
    {
        unsigned int wonTricks = tricksWon[currentPlayer->getName()];
        game.setResult(currentPlayer, wonTricks);
        currentPlayer = game.getNextPlayer(currentPlayer);
    }
}

void TerminalRomanianWhist::displayScoreboard()
{
    cout << "\n=========================" << endl;
    cout << "SCOREBOARD" << endl;
    cout << "=========================" << endl;
    cout << left << setw(20) << "Player" << setw(15) << "Round Score" << setw(15) << "Total Score" << endl;
    cout << "--------------------------------------------------------" << endl;
    
    auto roundScores = game.getPlayerRoundScores();
    
    for(const auto& [name, scores] : roundScores)
    {
        cout << left << setw(20) << name 
             << setw(15) << scores.first 
             << setw(15) << scores.second << endl;
    }
    cout << "=========================" << endl;
    
    // Show current leader
    auto playerScores = game.getPlayerScores();
    if(!playerScores.empty())
    {
        cout << "Current leader: " << playerScores[0].first << " with " << playerScores[0].second << " points" << endl;
    }
    cout << "=========================" << endl;
}

void TerminalRomanianWhist::displayFinalResults()
{
    cout << "\n=========================" << endl;
    cout << "FINAL RESULTS" << endl;
    cout << "=========================" << endl;
    cout << left << setw(20) << "Player" << setw(15) << "Total Score" << endl;
    cout << "--------------------------------------------------------" << endl;
    
    auto playerScores = game.getPlayerScores();
    
    for(const auto& [name, score] : playerScores)
    {
        cout << left << setw(20) << name << setw(15) << score << endl;
    }
    cout << "=========================" << endl;
    
    if(!playerScores.empty())
    {
        cout << "🏆 WINNER: " << playerScores[0].first << " with " << playerScores[0].second << " points! 🏆" << endl;
    }
    cout << "=========================" << endl;
}

