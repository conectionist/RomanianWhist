#include "Player.h"

#include <algorithm>
#include <utility>

Player::Player(const string &_name, 
               unique_ptr<IMoveProvider> _moveProvider) : name(std::move(_name)), 
                                                          moveProvider(std::move(_moveProvider)),
                                                          totalScore(0), 
                                                          currentRoundScore(0), 
                                                          consecutiveWins(0), 
                                                          consecutiveLosses(0)
{}

string Player::getName() const
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

const vector<Card*>& Player::getHand() const
{
    return hand;
}

Card* Player::playCard(Card* trump, const Suit* downSuit)
{
    // vector<Card*> legalCards = cardValidator.getLegalCards(hand, trump, downSuit);

    // // TODO: Temporary. Always play the first legal card; no strategy yet.
    // Card* card = legalCards[0];
    // hand.erase(std::find(hand.begin(), hand.end(), card));
    // return card;

    return moveProvider->playCard(hand, trump, downSuit);
}

bool Player::hasSuit(Suit suit) const
{
    for(const auto* card : hand)
    {
        if(card->suit == suit)
            return true;
    }

    return false;
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
