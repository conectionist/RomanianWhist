#include <romanian_whist/Player.h>

#include <algorithm>
#include <utility>

namespace romanian_whist
{
Player::Player(const std::string &_name, 
               std::unique_ptr<IMoveProvider> _moveProvider) : name(std::move(_name)), 
                                                          moveProvider(std::move(_moveProvider)),
                                                          totalScore(0), 
                                                          currentRoundScore(0), 
                                                          consecutiveWins(0), 
                                                          consecutiveLosses(0)
{}

std::string Player::getName() const
{
    return name;
}

void Player::addCardToHand(Card* card)
{
    hand.push_back(card);
}

void Player::clearHand()
{
    hand.clear();
}

const std::vector<Card*>& Player::getHand() const
{
    return hand;
}

Card* Player::playCard(Card* trump, const Suit* leadSuit)
{
    Card* card = moveProvider->playCard(hand, trump, leadSuit);
    if(card != nullptr)
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
    return card;
}

unsigned int Player::getBet(Card* trump, bool isFirstPlayer) const
{
    return moveProvider->makeBet(hand, trump, isFirstPlayer);
}

int Player::getTotalScore() const
{
    return totalScore;
}

int Player::getCurrentRoundScore() const
{
    return currentRoundScore;
}

void Player::addToScore(int points)
{
    currentRoundScore += points;
}

void Player::resetCurrentRoundScore()
{
    totalScore += currentRoundScore;
    currentRoundScore = 0;
}

void Player::incrementConsecutiveWins()
{
    consecutiveWins++;
    consecutiveLosses = 0;
}

void Player::incrementConsecutiveLosses()
{
    consecutiveLosses++;
    consecutiveWins = 0;
}

void Player::resetConsecutiveWins()
{
    consecutiveWins = 0;
}

void Player::resetConsecutiveLosses()
{
    consecutiveLosses = 0;
}

unsigned int Player::getConsecutiveWins() const
{
    return consecutiveWins;
}

unsigned int Player::getConsecutiveLosses() const
{
    return consecutiveLosses;
}

} // namespace romanian_whist
