#ifndef ROUND_H
#define ROUND_H

#include "Trick.h"
#include "PlayerList.h"
#include "Util.h"

#include <vector>
#include <unordered_map>

using std::vector;
using std::unordered_map;

struct Bet
{
    unsigned int guess;
    unsigned int actual;
};

class Round
{
private:
    vector<Trick> tricks;
    unordered_map<string, Bet> bets;
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
    
    unsigned int getBet(const string& playerName) const;
    unsigned int getActual(const string& playerName) const;
    bool hasBet(const string& playerName) const;
};

#endif
