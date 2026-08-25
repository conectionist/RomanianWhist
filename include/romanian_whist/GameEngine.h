#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include <romanian_whist/PlayerList.h>
#include <romanian_whist/Scoreboard.h>
#include <romanian_whist/Deck.h>

#include <string>
#include <vector>
#include <utility>

namespace romanian_whist
{
enum class GameStatus
{
    NotStarted,
    InProgress,
    Finished
};

class GameEngine
{
private:
    PlayerList players;
    Scoreboard scoreboard;
    Deck deck;
    GameStatus status;

public:
    GameEngine();
    void addPlayer(const std::string& name, std::unique_ptr<IMoveProvider> moveProvider);
    void initializeScoreboard(const GameStructure& structure, 
                              bool endWithForeheadAndHidden, 
                              bool all1GamesAreForehead);
    void initializeDeck(unsigned int playerCount);
    void setStatus(GameStatus _status);
    bool isInProgress() const;
    void shuffleDeck();
    void dealCards();
    Card* getCurrentTrumpCard();
    const Card* getCurrentTrumpCard() const;
    PlayerList::iterator getFirstPlayerOfTheRound();
    PlayerList::iterator getNextPlayer(PlayerList::iterator player);
    unsigned int getPlayerCount() const;
    void placeBet(PlayerList::iterator player, unsigned int bet);
    void setResult(PlayerList::iterator player, unsigned int wonTricks);
    unsigned int getCurrentRoundTrickCount() const;
    void addTrickToCurrentRound(const Trick& trick);
    PlayerList::iterator determineTrickWinner(const Trick& trick, PlayerList::iterator firstPlayer);
    void setFirstPlayerOfTheRound(PlayerList::iterator player);
    void completeCurrentRound();
    void calculateScores();
    void commitRoundScores();

    // Data access methods for display purposes
    std::vector<std::pair<std::string, int>> getPlayerScores() const;
    std::vector<std::pair<std::string, std::pair<int, int>>> getPlayerRoundScores() const;

    // Live game state, for clients that render more than a score table. Through
    // these a UI can read each seat's hand, streaks and scores, and the current
    // round's bets, results, trump and type - without keeping a parallel copy of
    // the game and hoping the two stay in step.
    //
    // Both hand out const access only. Note that Round::getFirstPlayer() and
    // getOpeningPlayer() still return mutable iterators, so a const Round does
    // not fully seal off the players behind it.
    const PlayerList& getPlayers() const;
    const Round& getCurrentRound() const;

    unsigned int getCurrentRoundIndex() const;
    unsigned int getRoundCount() const;
    RoundType getCurrentRoundType() const;

private:
    void clearAllPlayerHands();
    bool cardBeats(const Card& candidate, const Card& currentBest, Suit leadSuit);
};

} // namespace romanian_whist

#endif
