#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include <romanian_whist/PlayerList.h>
#include <romanian_whist/Scoreboard.h>
#include <romanian_whist/Deck.h>

#include <atomic>
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
    Finished,

    // requestStop() was honoured. A stopped game is not resumable.
    Stopped
};

// Where in a round the game is. Deliberately has no Stopped of its own: a stop
// lands at a trick boundary, so the phase stays Playing and the round stays
// unscored. getStatus() is what says the game is over; getPhase() says where in
// the round it stopped.
//
// RoundScored spans BOTH onRoundScored and onRoundComplete. An observer that
// needs to tell those apart uses the callback it is in, not getPhase().
enum class GamePhase
{
    NotStarted,
    Betting,
    Playing,
    RoundScored,
    GameOver
};

// Forward-declared rather than included: the engine only ever holds pointers to
// observers, and IGameObserver.h names GameEngine in return.
class IGameObserver;

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

    // Non-owning. Registration order is dispatch order.
    std::vector<IGameObserver*> observers;

    GamePhase phase = GamePhase::NotStarted;

    // Whose turn it is right now; disengaged between turns. Held here rather
    // than in a loop local for the same reason Round holds the in-flight trick.
    std::optional<Seat> activeSeat;

    // 1-based. 0 between the deal and the first trick.
    unsigned int currentTrickNumber = 0;

    // Read at each trick and round boundary. The only member another thread may
    // touch, and the reason GameEngine is neither copyable nor movable.
    std::atomic<bool> stopRequested{false};

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

    // Setting InProgress for the first time is what starts the game, and is
    // what fires onGameStarted() - so register every observer before calling
    // it. Phase 3 of ENGINE_V4_PLAN.md folds this into start(); until then it
    // is the client's job to remember.
    void setStatus(GameStatus _status);
    GameStatus getStatus() const;
    bool isInProgress() const;

    // Where in the round the game is. Callable at any time, including before
    // setup - GamePhase::NotStarted is the supported way for a client to ask
    // whether the round-scoped accessors are safe to call yet.
    GamePhase getPhase() const;

    // Non-owning. Observers must outlive the engine. Registering the same
    // observer twice is a no-op.
    //
    // Neither may be called from inside a callback: the engine iterates its
    // observer list while dispatching, so adding or removing mid-dispatch
    // invalidates that iteration. An observer that wants to detach sets a flag
    // and lets the client detach it between rounds.
    //
    // Unlike requestStop(), NEITHER IS THREAD-SAFE. The engine has no internal
    // locking anywhere, so calling these from a second thread while the game
    // thread is running races the same iteration - it is just harder to hit.
    void addObserver(IGameObserver* observer);
    void removeObserver(IGameObserver* observer);

    // ---- driving ----
    // Both block until the work is done, calling into move providers and
    // observers on the calling thread. Both throw std::logic_error unless the
    // status is InProgress, which is also what rejects a second run() on a
    // finished or stopped engine.
    //
    // If a provider or observer throws, the exception propagates out and the
    // engine is left mid-round and NOT resumable - destroy it. Nothing here is
    // exception-safe in the strong sense, and it does not need to be: the state
    // is already inconsistent and pretending otherwise is worse.
    void run();          // playRound() until the schedule runs out or a stop lands
    void playRound();    // deal, bet, play every trick, score, advance

    // Ends cleanly at the next trick boundary; status -> Stopped, and observers
    // get onGameStopped() rather than onGameOver(). Safe to call from another
    // thread; it only sets an atomic flag.
    //
    // It cannot interrupt a move provider already parked waiting for a human:
    // the flag is only read between tricks and rounds. Unparking that provider
    // is the client's job, and throwing from it is the supported way out.
    void requestStop();

    Card* getCurrentTrumpCard();
    const Card* getCurrentTrumpCard() const;

    // Who leads the next trick: the round leader until the first trick is won,
    // then each trick's winner.
    Seat getTrickLeaderSeat() const;

    // Who opened this round, fixed for its lifetime. Bidding runs from here,
    // wrapping round the table - which is what getBiddingOrder() below answers.
    //
    // Read the two apart carefully: until the first trick is won they name the
    // same seat, and from then on they do not. Bidding, and anything measured
    // from where the round began, wants this one.
    Seat getRoundLeaderSeat() const;

    // Where `seat` sits in this round's bidding order: 1 for the round leader,
    // then round the table. Saves every client reimplementing the modular
    // arithmetic, and getting it wrong once per client.
    unsigned int getBiddingOrder(Seat seat) const;

    Seat getNextSeat(Seat seat) const;
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

    unsigned int getCurrentRoundTrickCount() const;

    // Data access methods for display purposes
    std::vector<std::pair<std::string, int>> getPlayerScores() const;
    std::vector<std::pair<std::string, std::pair<int, int>>> getPlayerRoundScores() const;

    // Live game state, for clients that render more than a score table. Through
    // these a UI can read each seat's hand, streaks and scores, and the current
    // round's bets, results, trump and type - without keeping a parallel copy of
    // the game and hoping the two stay in step.
    //
    // Both hand out const access only. A Round names players by Seat, which
    // carries no access, so a const Round really does seal off the players
    // behind it.
    const PlayerList& getPlayers() const;
    const Round& getCurrentRound() const;

    // Read-only access to the deck, for inspecting composition and shuffle
    // order (tests) without a test-only friend declaration.
    const Deck& getDeck() const;

    unsigned int getCurrentRoundIndex() const;
    unsigned int getRoundCount() const;
    RoundType getCurrentRoundType() const;

    // ---- live round state ----
    // Whose turn it is; empty between turns. Engaged from just before a
    // provider is asked until just after its answer is recorded, so it is still
    // that seat inside onBetPlaced() and onCardPlayed() - which is what lets a
    // renderer keep the highlight on whoever just moved.
    std::optional<Seat> getActiveSeat() const;

    // 1-based; 0 after the deal and before the first trick.
    unsigned int getCurrentTrickNumber() const;

    // The trick in flight - see Round::getCurrentTrick() for exactly what it
    // holds when. Every entry carries the seat that played it, so a client
    // never has to reconstruct turn order arithmetically.
    const Trick& getCurrentTrick() const;

    // Who is winning the trick in flight, ranking whatever has been played so
    // far; empty before the first card. This is the engine's own ranking, so a
    // "currently winning" highlight can never disagree with the winner the
    // engine eventually declares.
    std::optional<Seat> getCurrentTrickLeader() const;

    // Empty until that seat has bid.
    std::optional<unsigned int> getBet(Seat seat) const;

    // Derived from the round's stored tricks, so it counts a trick from the
    // moment that trick is complete.
    unsigned int getTricksWon(Seat seat) const;

private:
    // ---- the loop's own primitives ----
    // These were the public API a client used to drive the game with, in an
    // order that was load-bearing and unenforced. playRound() is that order
    // now, so they are its business alone: a client outside the engine has no
    // way to call them out of sequence, and no reason to call them at all.
    void shuffleDeck();

    // Deals the current round's hands (and its trump card, below 8 tricks)
    // out of the deck. Requires both initializeDeck() and
    // initializeScoreboard() to have run - the deck supplies the cards and
    // the scoreboard the trick count - and throws std::logic_error
    // otherwise, rather than indexing an empty deck or an empty schedule.
    void dealCards();

    void placeBet(Seat seat, unsigned int bet);
    void addTrickToCurrentRound(const Trick& trick);

    // Ranks whatever has been played so far, so this also answers "who is
    // winning?" partway through a trick - which is what getCurrentTrickLeader()
    // exposes. Throws std::logic_error on a trick with no cards in it at all,
    // which has no answer to give.
    Seat determineTrickWinner(const Trick& trick) const;

    void setTrickLeaderSeat(Seat seat);
    void completeCurrentRound();
    void calculateScores();
    void commitRoundScores();

    // Every accessor whose answer depends on a current round routes through
    // Scoreboard::getCurrentRound(), which is `rounds[currentRound]` on a vector
    // that is empty until the scoreboard is laid out - so asking early is a read
    // of arbitrary memory, not a thrown error. One guard rather than a check per
    // accessor, so a future accessor cannot be added without one.
    //
    // Keyed on the scoreboard rather than on the status because that is the
    // actual precondition: a game whose status has been set but whose schedule
    // has not been built is exactly as unsafe. From Phase 3, start() does both
    // at once and the two conditions become the same thing.
    void requireStarted() const;

    // run() and playRound() need a live game. Also what stops a second run() on
    // an engine that has finished or been stopped.
    void requireInProgress() const;

    // The three stages of a round, kept separate so that each one's place in
    // the callback order is readable. All of them advance state that lives in
    // members - the phase, the active seat, the trick number, the round's
    // in-flight trick - rather than in locals of a nested loop, which is what
    // keeps a future non-blocking playStep() an additive change.
    void dealRound();
    void runBidding();
    void playTrick();

    // Returns true if the game has been stopped and the caller should give up.
    // Fires onGameStopped() exactly once.
    bool honourStopIfRequested();

    void clearAllPlayerHands();
    bool cardBeats(const Card& candidate, const Card& currentBest, Suit leadSuit) const;

    // One helper per callback, so the loop over `observers` lives in one place
    // and a dispatch site is a single line.
    void notifyGameStarted();
    void notifyRoundStarted();
    void notifyBetRequested(Seat seat);
    void notifyBetPlaced(Seat seat, unsigned int bet);
    void notifyBettingComplete();
    void notifyTrickStarted(unsigned int trickNumber, Seat leader);
    void notifyCardRequested(Seat seat);
    void notifyCardPlayed(Seat seat, const Card& card);
    void notifyTrickWon(Seat winner, unsigned int trickNumber);
    void notifyRoundScored();
    void notifyRoundComplete();
    void notifyGameOver();
    void notifyGameStopped();
};

} // namespace romanian_whist

#endif
