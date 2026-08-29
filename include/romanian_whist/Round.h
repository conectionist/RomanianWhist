#ifndef ROUND_H
#define ROUND_H

#include <romanian_whist/Trick.h>
#include <romanian_whist/PlayerList.h>
#include <romanian_whist/RoundType.h>

#include <cstddef>
#include <vector>
#include <unordered_map>

namespace romanian_whist
{
struct Bet
{
    unsigned int guess = 0;
    unsigned int actual = 0;

    // True only once setBet() has run for this player. setResult() writes
    // into the same map entry (to record tricks won as the round is played)
    // and must not be mistaken for a real bet by touching this - see
    // hasBet().
    bool guessSet = false;
};

class Round
{
private:
    std::vector<Trick> tricks;
    std::unordered_map<std::string, Bet> bets;
    Card* trump;
    unsigned int trickCount;
    RoundType type;

    // Who leads the next trick. Reassigned to each trick's winner as the round
    // plays out, so it stops identifying who opened the round after trick one.
    PlayerList::iterator firstPlayer;

    // Who led the round's first trick, fixed for the round's lifetime. This is
    // the one that determines bidding order.
    PlayerList::iterator openingPlayer;

public:
    Round(unsigned int _trickCount, PlayerList::iterator player, RoundType _type = RoundType::Normal);
    void addTrick(const Trick& trick);
    void setBet(PlayerList::iterator player, unsigned int guess);
    void setResult(PlayerList::iterator player, unsigned int wonTricks);
    void setTrumpCard(Card* card);
    Card* getTrumpCard();
    const Card* getTrumpCard() const;
    unsigned int getTrickCount() const;
    void setFirstPlayer(PlayerList::iterator player);

    // Note that both of these hand out a mutable iterator from a const Round.
    // Tightening them to const_iterator would break the game loop, which needs
    // a mutable player to deal to and to ask for a move.
    PlayerList::iterator getFirstPlayer() const;
    PlayerList::iterator getOpeningPlayer() const;

    void setRoundType(RoundType _type);
    RoundType getRoundType() const;

    std::size_t getPlayedTrickCount() const;

    // getBet() and getActual() return 0 for a player who has no entry, which is
    // indistinguishable from a genuine bet of zero. Ask hasBet() to tell them
    // apart - it is true only once setBet() has actually run for that player,
    // regardless of whether setResult() has touched their entry too.
    unsigned int getBet(const std::string& playerName) const;
    unsigned int getActual(const std::string& playerName) const;
    bool hasBet(const std::string& playerName) const;
};

} // namespace romanian_whist

#endif
