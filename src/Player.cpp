#include <romanian_whist/Player.h>

#include <algorithm>
#include <stdexcept>
#include <string>
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

void Player::addCardToHand(Card card)
{
    hand.push_back(card);
}

void Player::clearHand()
{
    hand.clear();
}

const std::vector<Card>& Player::getHand() const
{
    return hand;
}

std::optional<Card> Player::playCard(std::optional<Card> trump,
                                     std::optional<Suit> leadSuit,
                                     const std::vector<Card>& playedCards,
                                     unsigned int bet,
                                     unsigned int tricksWon)
{
    const std::optional<std::size_t> choice =
        moveProvider->playCard(PlayContext{hand, playedCards, trump, leadSuit, bet, tricksWon});

    if(!choice)
        return std::nullopt;

    if(*choice >= hand.size())
        throw std::logic_error(name + " chose card " + std::to_string(*choice)
                               + " of a hand holding " + std::to_string(hand.size()));

    // Erased by position, not by value: one card leaves the hand, and which one
    // is never in question. The std::remove this replaced erased every element
    // equal to the chosen one, which was only ever correct because a hand holds
    // no duplicates.
    const Card played = hand[*choice];
    hand.erase(hand.begin() + static_cast<std::ptrdiff_t>(*choice));

    return played;
}

unsigned int Player::getBet(std::optional<Card> trump,
                            bool isFirstPlayer,
                            std::optional<unsigned int> forbiddenBet) const
{
    return moveProvider->makeBet(BetContext{hand, trump, isFirstPlayer, forbiddenBet});
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
