#ifndef PLAYER_H
#define PLAYER_H

#include "Card.h"
#include "CardValidator.h"
#include "IMoveProvider.h"

#include <string>
#include <vector>
#include <memory>

using std::string;
using std::vector;
using std::unique_ptr;

class Player
{
private:
    string name;
    vector<Card*> hand;
    CardValidator cardValidator;
    int totalScore;
    int currentRoundScore;
    unsigned int consecutiveWins;
    unsigned int consecutiveLosses;
    unique_ptr<IMoveProvider> moveProvider;

public:
    Player(const string& _name, unique_ptr<IMoveProvider> _moveProvider);
    string getName() const;
    void addCardToHand(Card* card);
    void clearHand();
    const vector<Card*>& getHand() const;
    Card* playCard(Card* trump, const Suit* downSuit);
    bool hasSuit(Suit suit) const;
    unsigned int getBet(Card* trump, bool isFirstPlayer) const;
    
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

#endif
