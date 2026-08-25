#ifndef ROUND_H
#define ROUND_H

#include <romanian_whist/Trick.h>
#include <romanian_whist/PlayerList.h>
#include <romanian_whist/RoundType.h>

#include <vector>
#include <unordered_map>

namespace romanian_whist
{
struct Bet
{
    unsigned int guess;
    unsigned int actual;
};

class Round
{
private:
    std::vector<Trick> tricks;
    std::unordered_map<std::string, Bet> bets;
    Card* trump;
    unsigned int trickCount;
    RoundType type;
    PlayerList::iterator firstPlayer;

public:
    Round(unsigned int _trickCount, PlayerList::iterator player, RoundType _type = RoundType::Normal);
    void addTrick(const Trick& trick);
    void setBet(PlayerList::iterator player, unsigned int guess);
    void setResult(PlayerList::iterator player, unsigned int wonTricks);
    void setTrumpCard(Card* card);
    Card* getTrumpCard();
    unsigned int getTrickCount() const;
    void setFirstPlayer(PlayerList::iterator player);
    PlayerList::iterator getFirstPlayer() const;
    void setRoundType(RoundType _type);
    
    unsigned int getBet(const std::string& playerName) const;
    unsigned int getActual(const std::string& playerName) const;
    bool hasBet(const std::string& playerName) const;
};

} // namespace romanian_whist

#endif
