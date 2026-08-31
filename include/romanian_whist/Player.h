#ifndef PLAYER_H
#define PLAYER_H

#include <romanian_whist/Card.h>
#include <romanian_whist/IMoveProvider.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace romanian_whist
{
class Player
{
    // Asking a player to move is the engine's job, and only the engine's: the
    // two methods below are what the loop is made of, and a client that could
    // reach past GameEngine into a Player would be driving the game again.
    friend class GameEngine;

private:
    std::string name;
    std::unique_ptr<IMoveProvider> moveProvider;
    std::vector<Card*> hand;
    int totalScore;
    int currentRoundScore;
    unsigned int consecutiveWins;
    unsigned int consecutiveLosses;

public:
    Player(const std::string& _name, std::unique_ptr<IMoveProvider> _moveProvider);
    std::string getName() const;
    void addCardToHand(Card* card);
    void clearHand();
    const std::vector<Card*>& getHand() const;

    int getTotalScore() const;
    int getCurrentRoundScore() const;
    void addToScore(int points);
    void resetCurrentRoundScore();
    void incrementConsecutiveWins();
    void incrementConsecutiveLosses();
    void resetConsecutiveWins();
    void resetConsecutiveLosses();
    unsigned int getConsecutiveWins() const;
    unsigned int getConsecutiveLosses() const;

private:
    // As with getBet(), the hand is the player's own, so only the rest of the
    // context is passed in. `bet` and `tricksWon` come from the current Round.
    //
    // Note this MUTATES the hand: the chosen card is erased from it as part of
    // the same call, so anything that wants to judge the choice against the
    // hand has to have taken a copy first.
    Card* playCard(Card* trump,
                   const Suit* leadSuit,
                   const std::vector<Card*>& playedCards,
                   unsigned int bet,
                   unsigned int tricksWon);

    // The hand is the player's own, so only the rest of the bidding context is
    // passed in. `forbiddenBet` comes from GameEngine::getForbiddenBet().
    unsigned int getBet(Card* trump,
                        bool isFirstPlayer,
                        std::optional<unsigned int> forbiddenBet) const;
};

} // namespace romanian_whist

#endif
