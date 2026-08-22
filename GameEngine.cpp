#include "GameEngine.h"

#include <utility>

GameEngine::GameEngine() : status(GameStatus::NotStarted)
{}

void GameEngine::addPlayer(Player &&player)
{
    players.addPlayer(std::move(player));
}

void GameEngine::initializeScoreboard(const GameStructure &structure, 
                                      bool endWithForeheadAndHidden, 
                                      bool all1GamesAreForehead)
{
    scoreboard.initialize(structure, endWithForeheadAndHidden, all1GamesAreForehead, players);
}

void GameEngine::initializeDeck(unsigned int playerCount)
{
    for(int s = 0 ; s < 4 ; s++)
        for(int r = 1 + (6 - playerCount) * 2 ; r < 13 ; r++)
        {
            Card card(static_cast<Rank>(r), static_cast<Suit>(s));
            deck.addCard(std::move(card));
        }   
}

GameStatus GameEngine::getStatus()
{
    return status;
}

void GameEngine::setStatus(GameStatus _status)
{
    status = _status;
}

void GameEngine::start()
{
}

bool GameEngine::isInProgress()
{
    return status == GameStatus::InProgress;
}

void GameEngine::shuffleDeck()
{
    deck.shuffle();
}

void GameEngine::dealCards()
{
    clearAllPlayerHands();

    unsigned int gameCount = scoreboard.getCurrentRound().getHandCount();
    unsigned int index = 0;

    for(unsigned int i = 0 ; i < gameCount ; i++)
    {
        for(unsigned int j = 0 ; j < players.size() ; j++)
        {
            index = gameCount * j + i;
            players[j].addCardToHand(deck.getCardAt(index));
        }
    }

    if(gameCount < 8)
    {
        scoreboard.getCurrentRound().setTrumpCard(deck.getCardAt(index + 1));
    }
}

Card *GameEngine::getCurrentTrumpCard()
{
    return scoreboard.getCurrentRound().getTrumpCard();
}

void GameEngine::createPlayers(const vector<string> &playerNames)
{
    players.createPlayers(playerNames);
}

PlayerList::iterator GameEngine::getFirstPlayerOfTheRound()
{
    return scoreboard.getCurrentRound().getFirstPlayer();
}

PlayerList::iterator GameEngine::getNextPlayer(PlayerList::iterator player)
{
    return players.next(player);
}

unsigned int GameEngine::getPlayerCount()
{
    return players.size();
}

void GameEngine::placeBet(PlayerList::iterator player, unsigned int bet)
{
    scoreboard.getCurrentRound().setBet(player, bet);
}

void GameEngine::clearAllPlayerHands()
{
    for(auto& player : players)
        player.clearHand();
}
