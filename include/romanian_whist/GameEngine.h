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

    // True once initializeDeck() has built the deck. addPlayer(),
    // initializeDeck() and dealCards() all consult this - see their
    // declarations below.
    bool deckInitialized = false;

    // True once initializeScoreboard() has laid out the round schedule.
    // addPlayer(), initializeScoreboard() and dealCards() all consult this.
    bool scoreboardInitialized = false;

public:
    GameEngine();

    // Seeds the shuffle generator directly, for a reproducible deal. This
    // is the only thing the seed reaches: a move provider added afterwards
    // is still free to use its own randomness (RandomCardStrategy's default
    // constructor, for instance, still draws from std::random_device), so a
    // fully reproducible game additionally requires seeding every such
    // provider.
    explicit GameEngine(std::uint32_t seed);

    // Throws std::logic_error once initializeDeck() or
    // initializeScoreboard() has been called - both size their output to
    // the player count at the moment they run, so a seat added afterwards
    // leaves the deck sized for the wrong player count (see
    // initializeDeck()) and the round schedule laid out for the wrong one
    // (see initializeScoreboard()).
    void addPlayer(const std::string& name, std::unique_ptr<IMoveProvider> moveProvider);

    // Lays out the round schedule for the players added so far: its length
    // and the opener rotation both depend on the current player count, so
    // every seat must be in place first. Callable only once - Scoreboard
    // appends its rounds without clearing, so a second call would leave the
    // schedule twice as long with the opener rotation restarting halfway
    // through. A second call throws std::logic_error.
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

    // Deals the current round's hands (and its trump card, below 8 tricks)
    // out of the deck. Requires both initializeDeck() and
    // initializeScoreboard() to have run - the deck supplies the cards and
    // the scoreboard the trick count - and throws std::logic_error
    // otherwise, rather than indexing an empty deck or an empty schedule.
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
