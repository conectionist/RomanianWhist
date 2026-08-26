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
    // As with getBet(), the hand is the player's own, so only the rest of the
    // context is passed in. `bet` and `tricksWon` come from the current Round.
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
};

} // namespace romanian_whist

#endif
