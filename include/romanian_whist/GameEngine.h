#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include <romanian_whist/PlayerList.h>
#include <romanian_whist/Scoreboard.h>
#include <romanian_whist/Deck.h>

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

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
    std::mt19937 generator;

    // True once initializeDeck() has built the deck. addPlayer() and
    // initializeDeck() both consult this - see their declarations below.
    bool deckInitialized = false;

public:
    GameEngine();

    // Seeds the shuffle generator directly, for a reproducible deal. This
    // is the only thing the seed reaches: a move provider added afterwards
    // is still free to use its own randomness (RandomCardStrategy's default
    // constructor, for instance, still draws from std::random_device), so a
    // fully reproducible game additionally requires seeding every such
    // provider.
    explicit GameEngine(std::uint32_t seed);

    // Throws std::logic_error once initializeDeck() has been called - a
    // seat added afterwards would leave the already-built deck sized for
    // the wrong player count (see initializeDeck()).
    void addPlayer(const std::string& name, std::unique_ptr<IMoveProvider> moveProvider);
    void initializeScoreboard(const GameStructure& structure,
                              bool endWithForeheadAndHidden,
                              bool all1GamesAreForehead);

    // Builds the deck for playerCount players (2..6), which must match the
    // number of players already added. Callable only once: a second call is
    // rejected with std::logic_error rather than rebuilt, since a rebuild
    // after dealCards() has already handed out Card* into the old deck
    // would dangle every hand, the round's trump and any recorded tricks.
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
    // The bidding restriction: the final bidder may not make the round's bids
    // add up to exactly the trick count. getForbiddenBet() names the single bid
    // that would, and is empty for everyone but the final bidder - and empty
    // for them too when the bids already exceed the trick count, since no bid
    // can bring the total back down to it.
    //
    // Ask either of these before calling placeBet(), which records whatever it
    // is given without judging it.
    std::optional<unsigned int> getForbiddenBet() const;
    bool isBetLegal(unsigned int bet) const;

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

    // Read-only access to the deck, for inspecting composition and shuffle
    // order (tests) without a test-only friend declaration.
    const Deck& getDeck() const;

    unsigned int getCurrentRoundIndex() const;
    unsigned int getRoundCount() const;
    RoundType getCurrentRoundType() const;

private:
    void clearAllPlayerHands();
    bool cardBeats(const Card& candidate, const Card& currentBest, Suit leadSuit);
};

} // namespace romanian_whist

#endif
