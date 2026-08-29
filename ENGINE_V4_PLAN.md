# Engine v4: Move the game loop into the engine

A refactor plan for `RomanianWhistEngine`. Written to be handed to an implementer (human or AI)
and executed phase by phase.

---

## 1. Why

The engine models rounds, scoring and card ranking, but **not the play**. There is no
"play a trick" or "play a round" operation. So every client hand-writes the loop that
drives them, and that loop is made of game rules.

`romanian_whist_terminal/src/TerminalRomanianWhist.cpp` spans 194 lines across `loop()` and
`playCurrentRoundTricks()`, 124 of them executable. Strip the 41 `view.*` / `renderer.*` /
`pacer.*` lines and the remaining 83 are rules. The UI client decides the lead suit, tracks how
many tricks each player has won, tells the engine who leads next, and calls `calculateScores()` /
`commitRoundScores()` / `completeCurrentRound()` in an order that is load-bearing and
unenforced.

Three consequences:

**A second client duplicates all of it.** A planned web backend needs its own copy of the
same loop. Two copies of the rules can drift, and then two clients disagree about the same
game.

**The loop cannot be tested.** A full-game test would have to re-implement the loop inside
the test, so the test would validate a copy rather than the thing clients actually run. The
component most likely to contain a bug is currently the one component that is structurally
untestable. The engine has no automated tests at all today.

**Rules that need the loop have nowhere to live.** `RoundType::Forehead` and `RoundType::Hidden`
are stored but ignored — `IMPLEMENTATION_PLAN.md` lists this as an open gap. It has to be a gap,
because the *client* assembles the bidding prompt: `Player::getBet` is called from the client's
loop, so the engine has no hook at which to say a round is played blind, and no place to put the
rule about who may see whose cards. Once it owns the loop it has both (§3.6). Note this is about
the engine having somewhere to *state* the rule — it never gets to *enforce* it, since hands
stay publicly readable by necessity.

**The goal:** the engine owns the loop and publishes what happens; clients decide moves
(`IMoveProvider`, unchanged) and render events (`IGameObserver`, new).

---

## 2. What the current loop actually contains

From `playCurrentRoundTricks()`, with presentation removed:

```cpp
std::vector<unsigned int> tricksWon(playerCount, 0);          // shadow copy of engine state
for(trickIndex...)
{
    Trick trick;                                              // engine state, client-allocated
    const auto leader = game.getFirstPlayerOfTheRound();
    auto currentPlayer = leader;
    for(i < playerCount)
    {
        Card* playedCard = currentPlayer->playCard(trump, leadSuit, trick.getPlayedCards(),
                            game.getCurrentRound().getBet(currentPlayer->getName()),
                            tricksWon[seatOf(currentPlayer)]);
        if(playedCard == nullptr) throw ...;
        if(!trick.hasLeadSuit()) trick.setLeadSuit(playedCard->suit);
        trick.addPlayedCard(playedCard);
        currentPlayer = game.getNextPlayer(currentPlayer);
    }
    const auto winner = game.determineTrickWinner(trick, leader);
    trick.setWinner(winner);
    game.addTrickToCurrentRound(trick);
    game.setFirstPlayerOfTheRound(winner);
    tricksWon[seatOf(winner)]++;
    game.setResult(winner, tricksWon[seatOf(winner)]);
}
```

Specific problems visible here:

- **The client reaches past `GameEngine` into `Player`.** `currentPlayer->playCard(...)` assembles
  the engine's own `PlayContext` by hand. `GameEngine` is not involved.
- **Engine state is round-tripped through the client.** `getBet(name)` is read out of the engine
  and passed back in. `tricksWon` is a parallel copy — yet `addTrickToCurrentRound(trick)` stores
  each trick *with its winner*, so `Round` already holds everything needed to compute it.
- **`setResult()` writes into the same map as `setBet()`**, which is why `getForbiddenBet()` has to
  count bets by walking players and asking `hasBet()`, and why the client must seed results only
  after betting finishes.

### Three findings from reading the code

**The result-seeding loop in the terminal client is dead code.** `TerminalRomanianWhist.cpp:143-151`
seeds every seat's result to 0 with a comment explaining that a player who wins nothing would
otherwise never be written. But `Round::getActual()` already returns 0 for a missing entry
(`Round.cpp:89-97`), and `bets[name]` value-initialises `Bet` to zeros anyway. The loop is sequenced
after betting precisely to avoid making `hasBet()` true early, and by then every seat already
has an entry with `actual` at zero — so it changes nothing whatsoever. It should be deleted,
not ported.

**`Deck::shuffle()` cannot be seeded.** It constructs a `std::random_device` and `std::mt19937`
internally on every call (`Deck.cpp:13-18`). There is no way to make a game reproducible, so
**golden-output tests are impossible until this changes**. This is a prerequisite for Phase 0,
not a nice-to-have. `RandomCardStrategy` has the same problem (a fresh `random_device` per call),
so it must be excluded from deterministic tests or made seedable.

**Every completed round's stored cards silently change identity at the next shuffle.** This one is
a live bug, not a hazard, and it is the reason Phase 4 is not as optional as it looks.

`Scoreboard` keeps every `Round` it ever created (`rounds` is only ever appended to, and
`currentRound` is an index into it). Each `Round` holds a `Card* trump` and a `std::vector<Trick>`
whose tricks hold `std::vector<Card*>` — all of them pointing into `GameEngine::deck`, which is
built **once** by `initializeDeck` and re-`shuffle()`d at the top of every round. `std::shuffle`
permutes values between slots, so the pointers stay valid and start denoting different cards.
From round two onward, **every finished round's recorded trump and recorded tricks are wrong**,
and they keep changing for the rest of the game.

Nothing reads them today, which is why nobody has seen it: the terminal client touches history
only through `getRoundCount()`. That is exactly the kind of dormancy that ends when a second
client arrives — "show me the previous round" is an unremarkable thing for a Qt window or a web
scoreboard to want, and it is a feature that looks like pure presentation work right up until it
renders nonsense.

Two consequences for the plan:

- **Phase 4 is a bug fix, not only a safety refactor.** Holding cards by value is what makes a
  retained `Round` mean what it says. §6's scope note is adjusted accordingly.
- **"Phase 4 must not alter play at all" is still true, and is not the whole guarantee.** Play
  does not change, so the golden *scores* hold. But what a completed round reports about itself
  does change — from garbage to correct — so any test that asserts over round history has to be
  written after Phase 4, not before. Phase 0's goldens assert final scores and per-round
  bid/actual, both of which are read live rather than out of history, so they are unaffected.

---

## 3. Target architecture

The seam moves from *"engine holds state, client drives"* to *"engine drives, client decides and
observes"*.

| Interface | Status | Responsibility |
|---|---|---|
| `IMoveProvider` | unchanged in role | "What do you want to do?" — bet and card decisions |
| `IGameObserver` | **new** | "Here is what just happened" — rendering, logging, broadcasting |
| `GameEngine::run()` / `playRound()` | **new** | owns the loop |

"Unchanged in role" is not "unchanged in signature": `IMoveProvider` keeps its job of answering
"what do you want to do?", but Phase 4 changes `playCard` to return `std::optional<std::size_t>`
and rewrites both context structs to hold cards by value. The seam stays where it is; the types
crossing it do not.

### 3.1 `Seat` replaces `PlayerList::iterator` in the public API

```cpp
// include/romanian_whist/Seat.h
namespace romanian_whist
{
struct Seat
{
    unsigned int index;

    explicit constexpr Seat(unsigned int _index) : index(_index) {}
};

constexpr bool operator==(Seat left, Seat right);
constexpr bool operator!=(Seat left, Seat right);
}
```

**A struct, not `using Seat = unsigned int`.** A seat is always passed next to a count that looks
identical to it — `placeBet(Seat, unsigned int bet)`, `Round(unsigned int trickCount, Seat opener,
unsigned int seatCount)` — and with an alias every swapped call still compiles. The iterator this
replaces made those a compile error, and an alias would quietly give that back at exactly the
moment the client's call sites are being rewritten. `index` is public because a seat *is* an
index: it keys `Round::bets` and `PlayerList`, so `bets[seat.index]` says what an accessor would,
only shorter. Loops over the table read `for(unsigned int i = 0 ; i < count ; i++) { Seat seat{i};
… }`.

Iterators become an internal detail of `PlayerList`. This deletes `seatOf()` from every client,
and lets `Round.h` and `Trick.h` stop including `PlayerList.h` entirely — a real decoupling, since
`Trick` currently drags in `PlayerList` → `Player` → `IMoveProvider` just to name a winner.

### 3.2 `IGameObserver`

```cpp
// include/romanian_whist/IGameObserver.h
namespace romanian_whist
{
class GameEngine;

// Called synchronously, on the thread running the game, in the order events occur.
// Every method is a no-op by default so a client implements only what it renders.
//
// Blocking inside a callback pauses the game. That is intentional - it is where
// pacing and "press Enter to continue" belong.
//
// A callback receives `const GameEngine&`, which is only safe to read on the
// thread running the game. A client whose UI lives on another thread must
// snapshot by value inside the callback and hand the snapshot across - see
// section 3.8.
class IGameObserver
{
public:
    virtual ~IGameObserver() = default;

    virtual void onGameStarted(const GameEngine&) {}
    virtual void onRoundStarted(const GameEngine&) {}          // dealt; trump known
    virtual void onBetRequested(const GameEngine&, Seat) {}
    virtual void onBetPlaced(const GameEngine&, Seat, unsigned int bet) {}
    virtual void onBettingComplete(const GameEngine&) {}
    virtual void onTrickStarted(const GameEngine&, unsigned int trickNumber, Seat leader) {}
    virtual void onCardRequested(const GameEngine&, Seat) {}
    virtual void onCardPlayed(const GameEngine&, Seat, const Card&) {}
    virtual void onTrickWon(const GameEngine&, Seat, unsigned int trickNumber) {}
    virtual void onRoundScored(const GameEngine&) {}           // scored, not yet committed
    virtual void onRoundComplete(const GameEngine&) {}         // committed
    virtual void onGameOver(const GameEngine&) {}        // played to the end
    virtual void onGameStopped(const GameEngine&) {}     // requestStop() honoured
};

} // namespace romanian_whist
```

Every callback takes `const GameEngine&` so an observer never needs to capture state or keep a
parallel copy — it reads what it needs at the moment it is told something changed.

#### Exactly when each one fires

An observer reads engine state *during* the callback, so "just before" and "just after" are not
stylistic details — they decide what it sees. Pin them here rather than letting Phase 2 decide by
accident:

| Callback | Fires | State visible inside it |
|---|---|---|
| `onGameStarted` | at the end of `start()`, not from `run()` | the schedule exists and round 0 is already constructed, so `requireStarted()` passes and every round-scoped accessor answers normally — trick count, round type and `getRoundLeaderSeat()` are all live. What is not true yet is that the round has been *dealt*: trump and every bet are empty and `getCurrentTrickNumber() == 0`. §3.3's pre-`start()` rule keys on `NotStarted` and no longer applies by this point, so do **not** add a second guard here that makes these throw |
| `onRoundStarted` | after the deal, after the in-flight trick is reset | `getCurrentRoundIndex()` is **this** round; trump known; `getCurrentTrickNumber() == 0` |
| `onBetRequested` | before the provider is asked | `getActiveSeat()` is that seat; its bet is still empty |
| `onBetPlaced` | after the bet is recorded | that seat's `getBet()` is engaged |
| `onBettingComplete` | after the last bet | every seat's `getBet()` is engaged; `getActiveSeat()` empty |
| `onTrickStarted` | after the in-flight trick is reset | `getCurrentTrick()` is **empty**; `getCurrentTrickNumber()` is this trick |
| `onCardRequested` | before the provider is asked | `getActiveSeat()` is that seat; its card is not yet in `getCurrentTrick()`, so `getCurrentTrickLeader()` names who is winning *without* it |
| `onCardPlayed` | after the card joins the in-flight trick | the card is in `getCurrentTrick()` |
| `onTrickWon` | after the completed trick is appended to the round, before it is cleared | `getTricksWon(winner)` **includes** this trick; `getCurrentTrick()` is still the finished trick — see Phase 2 step 3 |
| `onRoundScored` | between `calculateScores()` and `commitRoundScores()` | `getCurrentRoundIndex()` is **still this round**; `getRoundScore()` is what the round was worth, `getTotalScore()` the running total it has not yet folded into |
| `onRoundComplete` | after `commitRoundScores()`, and **after** `completeCurrentRound()` advances the index | `getCurrentRoundIndex()` is the **next** round — or, on the last round, the status is `Finished` and the index stays put |
| `onGameOver` / `onGameStopped` | once, after the loop leaves | terminal state |

Two of these are easy to get wrong and worth stating twice:

- **`onRoundScored` is the round-end render hook, not `onRoundComplete`.** It is the last callback
  at which `getCurrentRoundIndex()` still names the round being reported. An observer that draws
  "Round 5 of 26 — here is what it was worth" from `onRoundComplete` draws round 6's number over
  round 5's scores. The terminal's round-end screen (`TerminalRomanianWhist.cpp:157-167`) renders
  before commit today, so porting it to `onRoundScored` is a faithful port; porting it to
  `onRoundComplete` is an off-by-one.
- **`onGameStarted` belongs to `start()`, not `run()`.** A client that registers observers, calls
  `start()` and then hands the engine to a worker thread to `run()` still wants the "game begins"
  event on the setup path, and `run()` may be called more than once in principle (it resumes a
  partly played game). Firing it from `run()` would either duplicate it or misplace it.

### 3.3 `GameEngine`, after

```cpp
enum class GameStatus { NotStarted, InProgress, Finished, Stopped };
enum class GamePhase  { NotStarted, Betting, Playing, RoundScored, GameOver };

struct SeatSetup
{
    std::string name;
    std::unique_ptr<IMoveProvider> moveProvider;
};

struct GameSetup
{
    std::vector<SeatSetup> seats;                  // 2..6, validated
    GameStructure structure = GameStructure::S_181;
    bool endWithForeheadAndHidden = true;
    bool all1GamesAreForehead = false;

    // Set for a reproducible game. Unset seeds from std::random_device.
    std::optional<std::uint32_t> shuffleSeed;
};

struct Standing { Seat seat; std::string name; int score; };

class GameEngine
{
public:
    GameEngine();

    // Out-of-line so a member can later become an incomplete type without
    // GameEngine.h having to include its definition. If nothing here needs
    // that, delete the line rather than writing an empty body: declaring a
    // destructor is already what suppresses the implicit move, so the
    // deletions below would then be documenting what the compiler did anyway.
    ~GameEngine();
    GameEngine(const GameEngine&)            = delete;
    GameEngine& operator=(const GameEngine&) = delete;
    GameEngine(GameEngine&&)                 = delete;   // no client needs to move a live
    GameEngine& operator=(GameEngine&&)      = delete;   // engine - see the note below

    // ---- setup ----
    // Validates (2..6 seats; names non-empty and unique) and performs every setup
    // step in the one correct order. Throws std::invalid_argument on bad input,
    // and std::logic_error unless the status is NotStarted - initializeDeck()
    // appends unconditionally, so a second start() deals from a double-sized
    // deck rather than failing.
    //
    // GameSetup holds unique_ptrs and is move-only, so every call site reads
    // start(std::move(setup)).
    void start(GameSetup setup);

    // Non-owning. Observers must outlive the engine. Registering the same
    // observer twice is a no-op.
    //
    // Neither may be called from inside a callback. The engine iterates its
    // observer list while dispatching, so adding or removing mid-dispatch
    // invalidates that iteration. An observer that wants to detach sets a flag
    // and lets the client detach it between calls to playRound().
    //
    // Unlike requestStop(), NEITHER IS THREAD-SAFE. §3.8 point 1 means what it
    // says: the engine has no internal locking anywhere, not only around the
    // loop, so calling these from a second thread while run()/playRound() is
    // executing on the game thread races the same iteration as the mid-callback
    // case above - it is just harder to hit. Registration happens on the game
    // thread, before run() and between playRound() calls, same as everything
    // else in §3.8.
    void addObserver(IGameObserver* observer);
    void removeObserver(IGameObserver* observer);

    // ---- driving ----
    // Both block until the work is done, calling into move providers and
    // observers on the calling thread. See section 3.8 for what that obliges a
    // client to do. If a provider or observer throws, the exception propagates
    // out and the engine is left NOT resumable - destroy it.
    void run();          // while(isInProgress() && !stopRequested) playRound();
    void playRound();    // deal, bet, play every trick, score, advance

    // Ends cleanly at the next trick boundary; status -> Stopped, and observers
    // get onGameStopped() rather than onGameOver(). Safe to call from another
    // thread; it only sets an atomic flag.
    //
    // It cannot interrupt a move provider already parked waiting for a human:
    // the flag is only read between tricks and rounds. Unparking that provider
    // is the client's job, and throwing from it is the supported way out - see
    // section 3.8 point 4.
    void requestStop();

    // ---- status ----
    bool         isInProgress() const;
    GameStatus   getStatus() const;
    GamePhase    getPhase() const;

    // ---- live round state ----
    std::optional<Seat> getActiveSeat() const;      // empty between turns
    Seat         getRoundLeaderSeat() const;        // fixed for the round; sets bidding order
    Seat         getTrickLeaderSeat() const;        // moves to each trick's winner
    unsigned int getBiddingOrder(Seat) const;       // 1-based; 1 == round leader
    unsigned int getCurrentTrickNumber() const;     // 1-based; 0 before the first trick
    const Trick& getCurrentTrick() const;           // the in-flight trick

    // Who is winning the in-flight trick, ranking whatever has been played so
    // far; empty before the first card. This is the engine's own ranking, so a
    // "currently winning" highlight can never disagree with the winner the
    // engine eventually declares. It is what replaces the terminal client's
    // markCurrentlyWinning(), which reached determineTrickWinner() directly.
    std::optional<Seat> getCurrentTrickLeader() const;
    std::optional<unsigned int> getBet(Seat) const; // empty until that seat has bid
    unsigned int getTricksWon(Seat) const;          // derived from the round's stored tricks

    // ---- round / game shape ----
    unsigned int getPlayerCount() const;
    unsigned int getCurrentRoundIndex() const;
    unsigned int getRoundCount() const;
    unsigned int getCurrentRoundTrickCount() const;
    RoundType    getCurrentRoundType() const;
    std::optional<Card> getCurrentTrumpCard() const;  // empty in 8-card rounds
                                                      // (const Card* until Phase 4)
    const PlayerList& getPlayers() const;
    const Round&      getCurrentRound() const;

    // ---- hand visibility ----
    // May `viewer` see `holder`'s hand in the current round? Normal: only your
    // own. Forehead: everyone's but your own. Hidden: nobody's. One rule, in
    // the engine, so two clients cannot disagree about it - see section 3.6.
    bool canSeeHand(Seat viewer, Seat holder) const;

    // ---- bidding rules, for prompting a UI ----
    std::optional<unsigned int> getForbiddenBet() const;
    bool isBetLegal(unsigned int bet) const;

    // ---- scores ----
    std::vector<Standing> getStandings() const;     // sorted desc; carries seat, so
                                                    // duplicate names stay distinguishable

    // getRoundScore() is what the current round is worth so far. getTotalScore()
    // is the *committed* total and does not include it until commitRoundScores()
    // folds it in - Player::addToScore() writes currentRoundScore, and
    // resetCurrentRoundScore() is the commit (Player.cpp:66-75).
    //
    // A scoreboard wanting the projected total - which is what
    // getPlayerRoundScores() returned as its second element, and what the
    // terminal renders today - must add the two: getTotalScore(s) +
    // getRoundScore(s). Rendering getTotalScore() alone shows a different
    // number inside onRoundScored than inside onRoundComplete, for the same
    // round. See Phase 3 point 6.
    int getRoundScore(Seat) const;
    int getTotalScore(Seat) const;
};
```

**Why the move constructor is deleted, precisely.** It is not fixing an aliasing hazard: `PlayerList`
(a `std::vector` from Phase 1), `Scoreboard` and `Deck` all move by relocating their internal
buffer, not their elements, so a `Card*` or `Seat` held elsewhere stays valid across a `GameEngine`
move even pre-Phase-4. The real reason is narrower — nothing in this plan ever needs to move a live
engine, and deleting the operation is cheaper than auditing every current and future member (a
registered `IMoveProvider*`, an observer that captured `this`, a future worker-thread handle) for
whether moving it out from under them is safe. If a client ever needs a movable engine, that is the
point to re-derive the answer, not to assume this comment already did.

**This is not a hypothetical deferred to some future client — the web backend named in §1 and
§3.8 needs the answer now.** A backend hosting several concurrent games cannot put a
non-movable, non-copyable `GameEngine` in a `std::vector<GameEngine>` or return one by value from
a factory. It has to hold each game behind a pointer from the first line of that code —
`std::map<GameId, std::unique_ptr<GameEngine>>` or equivalent — so the game-hosting container is
heap-allocating `GameEngine` per entry, never storing it inline. This follows directly from the
deletions above; it is written down here so the web backend's author reaches for `unique_ptr`
from the start instead of rediscovering the constraint from a compile error.

#### `GamePhase` transitions, in full

`getPhase()` is public, and Phase 2 *gates* `calculateScores()` on it (see the double-scoring
guard there), so its transitions are part of the contract rather than an artefact of how the loop
gets written. Pin them here, the same way §3.2 pins the callbacks:

| From | To | On |
|---|---|---|
| `NotStarted` | `Betting` | the deal, at the top of `playRound()` — so `Betting` is what an observer sees inside `onRoundStarted` |
| `Betting` | `Playing` | the last bet is recorded, before `onBettingComplete` fires |
| `Playing` | `RoundScored` | `calculateScores()`, which refuses to run from any other phase |
| `RoundScored` | `Betting` | the next round's deal |
| `RoundScored` | `GameOver` | `completeCurrentRound()` finding no next round |

`RoundScored` therefore spans **both** `onRoundScored` and `onRoundComplete`. The phase does not
separate them; an observer that needs to tell them apart uses the callback it is in, not
`getPhase()`.

**A stopped game deliberately has no phase of its own.** `GameStatus::Stopped` is not mirrored in
`GamePhase`: a stop lands at a trick boundary, so the phase stays `Playing` and the round stays
unscored. `getStatus()` is what says the game is over; `getPhase()` says where in the round it
stopped. A `GamePhase::Stopped` would be a second enum answering the first one's question, and
the two would eventually disagree.

#### Every round-scoped accessor needs a defined answer before `start()`

`Scoreboard::getCurrentRound()` is `return rounds[currentRound];` on a vector that is empty until
the scoreboard is initialised, so indexing it before setup is undefined behaviour. Today that is
invisible: `GameEngine`'s only consumer is a client that does its setup and its reads in one
hand-written sequence, and never asks a question out of order.

This section adds roughly twenty new accessors that all route through that same call —
`getCurrentRoundType`, `getBet`, `getTricksWon`, `getCurrentTrick`, `getCurrentTrickLeader`,
`canSeeHand`, `getCurrentRoundTrickCount`, `getForbiddenBet`, and the rest — and hands them to
clients with a setup *screen*. A Qt window that renders a scoreboard widget before the wizard
finishes, or a web backend that serves `GET /game/{id}` on a game that was created but never
started, walks straight into it. The failure is a read of arbitrary memory, not a thrown error.

**The rule:** every accessor whose answer depends on a current round throws `std::logic_error`
when `getStatus() == GameStatus::NotStarted`. `getStatus()`, `getPhase()`, `isInProgress()` and
`getPlayerCount()` stay callable at all times and are how a client asks whether the rest are safe
— `getPhase()` returning `GamePhase::NotStarted` is the supported way to find out.

Implement it as one private `requireStarted()` guard rather than a check per accessor, so a
future accessor cannot be added without one. Phase 2 adds the guard alongside the accessors; Phase 3
keeps it once `start()` is what sets the status.

**Two of these have no caller yet.** `getBiddingOrder(Seat)` and `Trick::getCardPlayedBy(Seat)`
(§3.5) are not used by any phase or client migration in this plan. Every accessor added now costs a
`requireStarted()` guard and a row of test coverage, and one with no consumer is one whose contract
nobody has checked against a real need. Either name the caller — a Qt bidding panel and a web
`GET /game/{id}` payload are the plausible ones — or leave them out until one exists.

**Removed from the public API** (all become private implementation details of `playRound()`):
`shuffleDeck`, `dealCards`, `placeBet`, `addTrickToCurrentRound`, `determineTrickWinner`,
`setRoundLeaderSeat`, `completeCurrentRound`, `calculateScores`, `commitRoundScores`,
`initializeScoreboard`, `initializeDeck`, `setStatus`, `getRoundLeaderSeat`, `getNextSeat`.

**This list is written against the post-Phase-1 API.** Earlier drafts named `setResult`,
`setFirstPlayerOfTheRound`, `getFirstPlayerOfTheRound` and `getNextPlayer`; the first no longer
exists at all and the other three are the `Seat` names above.

**`getPlayer(Seat)` goes with them, and it is the one that matters.** Phase 1 added it as a
deliberate temporary: converting the accessors to `Seat` closed the only door handing out a
non-const `Player&`, which a caller still driving the loop needs to reach the non-const
`playCard()`. Once `playRound()` is that caller, nothing outside the engine has any business
with a mutable `Player` — and leaving it reopens the const-correctness hole §3.1 closes,
permanently and silently, because nothing fails when an accessor merely survives.

`Player::playCard` and `Player::getBet` become private with `friend class GameEngine`, so no
client can reach past the engine into a player again.

### 3.4 `Round`, after

```cpp
class Round
{
private:
    std::vector<Trick> tricks;                        // completed
    Trick currentTrick;                               // in flight
    std::vector<std::optional<unsigned int>> bets;    // indexed by Seat
    const Card* trump = nullptr;
    unsigned int trickCount;
    RoundType type;
    Seat trickLeaderSeat;   // moves to each trick's winner
    Seat roundLeaderSeat;   // fixed for the round; sets bidding order

public:
    std::optional<unsigned int> getBet(Seat) const;
    unsigned int getTricksWon(Seat) const;   // counts stored tricks whose winner == seat
    const Trick& getCurrentTrick() const;
    std::size_t  getPlayedTrickCount() const;
    // ...
};
```

**Name the members after the accessors that expose them.** `getRoundLeaderSeat()` is the fixed one
and `getTrickLeaderSeat()` the moving one (§3.3), so the members are `roundLeaderSeat` and
`trickLeaderSeat` — not `openerSeat` and `leaderSeat`, where a bare "leader" would mean the moving
one as a member and the fixed one as an accessor. §3.5 and Phase 2 step 3 both warn that confusing
these two silently draws every card against the wrong seat; leaving them near-homonyms is how that
confusion gets in.

**`Bet::actual`, `Round::setResult()`, `Round::getActual()` and `Round::hasBet()` are deleted.**
Tricks won are *derived* from the tricks the round already stores, not pushed in by a client.
This removes the `setBet`/`setResult` shared-storage coupling outright, so `getForbiddenBet()`
becomes a straightforward count over `bets` and no longer needs its defensive walk.

`getBet()` returning `std::optional` removes the "0 means either a zero bid or no bid" trap;
`hasBet()` has nothing left to disambiguate.

### 3.5 `Trick`, after

```cpp
struct PlayedCard
{
    Seat seat;
    Card card;
};

class Trick
{
private:
    std::vector<PlayedCard> playedCards;   // cards by value - see Phase 4
    std::optional<Suit> leadSuit;          // empty until the first card is played
    std::optional<Seat> winner;            // empty until the trick is decided

public:
    const std::vector<PlayedCard>& getPlayedCards() const;
    std::optional<Card> getCardPlayedBy(Seat) const;   // empty if that seat has not played yet
};
```

Storing `Seat` instead of `PlayerList::iterator` also fixes a latent problem: `Trick`'s default
constructor currently leaves `winner` singular, and `addTrickToCurrentRound` copies the trick —
copying a singular iterator is not something to rely on.

#### Why the trick carries seats, and not just cards

A trick that stores bare cards in play order is only readable if you also know who led it and
count round the table from there. Every client that draws a table would then compute
`(trickLeaderSeat + i) % playerCount` for itself — turn-order arithmetic, in the client, which is
exactly the category §1 says has to move into the engine. One reimplementation of it per client
is one chance per client to get it wrong.

It is not a theoretical risk. `getTrickLeaderSeat()` **moves to each trick's winner**, so it is
already the *next* leader by the time `onTrickWon` fires — while the finished trick is still on
screen and still being drawn (Phase 2 step 3). A client rebuilding its table from
`(leader + i) % playerCount` inside that callback silently renders every card against the wrong
seat, on the one screen the player actually stops and reads. §6 notes the goldens cover rules and
not presentation, so nothing in the test suite would catch it.

Carrying the seat removes the arithmetic instead of documenting it, and it pays for itself twice
more: `determineTrickWinner` stops needing `players.advanceCircular(firstPlayer, bestIndex)` to
turn a winning *index* back into a seat — it reads the winner's seat straight off the winning
entry — and `Round::getTricksWon(Seat)` becomes a count over `winner` fields with no positional
reasoning anywhere in it.

### 3.6 Round types: what the engine says, and what it does not enforce

```cpp
struct BetContext
{
    const std::vector<Card>& hand;      // always the real hand; never blinded
    std::optional<Card> trump;          // empty in 8-card rounds (Phase 4; Card* until then)
    bool isFirstPlayer = false;
    std::optional<unsigned int> forbiddenBet;
    RoundType roundType = RoundType::Normal;   // added in Phase 5
    Seat seat = 0;
};
```

**Append new fields at the end.** `Player::getBet` builds this positionally
(`BetContext{hand, trump, isFirstPlayer, forbiddenBet}`, `Player.cpp:53`), so inserting a field
in the middle silently misassigns every one after it. **Switch that call site to designated
initialisers in Phase 0**, along with `Player::playCard`'s `PlayContext{...}` (`Player.cpp:43`). It
is one line each on C++20 and it retires the hazard outright, which is worth more than carrying a
warning about it through five more phases.

`PlayContext` gains `Seat seat` and is otherwise unchanged.

#### The engine does not blind the hand — and could not

An earlier draft of this plan had the engine pass an **empty** `hand` in Forehead and Hidden
rounds, with a separate `handSize` carrying the legal bid range. That is dropped, for two
reasons.

**It would not have enforced anything.** `GameEngine::getPlayers()` and `Player::getHand()` are
public, and they have to stay public — the renderer reads them to draw every seat's cards
(`GameView.cpp:60,80-81`), which is exactly what a Forehead round needs. Blinding one struct
while the same cards sit one accessor away is a speed bump, not a wall.

**It cost more than the rule is worth.** Every Forehead and Hidden round has a trick count of
one, so the hand holds a single card and playing it is not a decision at all — `PlayContext`
needs nothing. Only bidding is affected, and only for a strategy that reads its hand to bid:

| Strategy | Bids on a blind hand | Change |
|---|---|---|
| `FirstCardStrategy` | `forbiddenBet == 0 ? 1 : 0` — never reads the hand | none |
| `DuckingStrategy` | identical | none |
| `RandomCardStrategy` | draws over `[0, hand.size()]`, stepping over a forbidden value; reads the hand's *size*, never its contents | none |
| `LowRiskStrategy` | reads the hand via `countLikelyWinners` | **the only one** |

So Phase 5 is one `if` in one strategy, not a new contract every strategy has to honour.

#### `canSeeHand()` — the rule, in one place

What *is* worth putting in the engine is the visibility rule itself, because both clients need
it and §1's argument applies to it exactly: a rule each client reimplements is a rule two
clients can disagree about. Note the question is broader than the special round types — in a
Normal round a backend must already send seat A only A's cards. Forehead merely inverts it.

```cpp
bool GameEngine::canSeeHand(Seat viewer, Seat holder) const
{
    switch(getCurrentRoundType())
    {
        case RoundType::Forehead: return viewer != holder;
        case RoundType::Hidden:   return false;
        case RoundType::Normal:   break;
    }
    return viewer == holder;
}
```

The renderer stops deciding and starts asking. A spectator is not a seat, so "a spectator sees
everything" stays a client policy and simply does not consult this.

This is still not enforcement, and the plan should not claim otherwise: round-type blinding is a
presentation rule. What `canSeeHand()` buys is one correct implementation of it, in the place
both clients already look, and something Phase 0e can unit-test.

#### Known gap: Forehead and Hidden are identical to an AI

In a Forehead round a bidder cannot see their own card but *can* see everyone else's — and
`BetContext` has no field for other seats' cards, so a strategy cannot use them. Both round
types therefore mean the same thing to an AI: bid on nothing.

This is accepted deliberately. None of the four strategies is sophisticated enough to use that
information today. When one is, `BetContext` grows a field for the visible cards (appended, per
above) and no existing strategy breaks. Do not read Phase 5 as making Forehead *play*
differently from Hidden — it makes them display differently, and that is all it claims.

### 3.7 What the terminal client becomes

> **This is the shape after Phase 3, not after Phase 2.** `start()`, `GameSetup` and
> `getStandings()` all arrive in Phase 3, so an implementer who follows Phase 2 step 9 and copies
> this verbatim will reach for three things that do not exist yet. Phase 2's interim shape is
> written out in that step; the difference is confined to `startGame()`, and every observer
> callback below is final from Phase 2 on.

```cpp
void TerminalRomanianWhist::startGame()
{
    game.addObserver(this);
    game.start(buildSetup(options.demo ? SetupWizard::demoConfig() : wizard.run()));
    game.run();

    // Off the alternate screen first, so the result is still there after the
    // program exits. Unchanged from today, and it stays correct on the throwing
    // path for free - see below.
    terminal.leaveAltScreen();
    renderer.drawGameOver(game.getStandings());
}

void TerminalRomanianWhist::onCardPlayed(const GameEngine& g, Seat seat, const Card&)
{
    refreshFromEngine(view, g);
    renderer.drawFrame(view);
    if(static_cast<int>(seat) != view.humanSeat) pacer.beat();
}

void TerminalRomanianWhist::onTrickWon(const GameEngine& g, Seat seat, unsigned int trickNumber)
{
    view.message = g.getPlayers().at(seat).getName() + " wins the trick.";
    refreshFromEngine(view, g);
    renderer.drawFrame(view, trickNumber < g.getCurrentRoundTrickCount()
                             ? "Press Enter for the next trick"
                             : "Press Enter to see the round result");
    pacer.trickPause();
}
```

`loop()`, `playCurrentRoundTricks()` and `seatOf()` are deleted. `openingSeat` and the `tricksWon`
vector are deleted. `refreshFromEngine` loses its `openingSeat` parameter and becomes a pure
function of the engine.

`markCurrentlyWinning()` **survives** — it is a rendering decision, not a rule — but it stops
calling `determineTrickWinner()` (now private) and reads `getCurrentTrickLeader()` instead:

```cpp
void TerminalRomanianWhist::markCurrentlyWinning(const GameEngine& g)
{
    const std::optional<Seat> leading = g.getCurrentTrickLeader();
    for(PlayedCardView& played : view.table)
        played.isWinning = leading && played.seatIndex == *leading;
}
```

Fold it into `refreshFromEngine` if you prefer; the point is that the highlight must not be lost,
and must not be reimplemented in the client.

`GameView`, `Renderer`, `CardFormat`, `SetupWizard`, `Terminal`, `Input` and `ConsoleMoveProvider`
are untouched.

#### `run()` can throw, and only the terminal already copes

§3.8 point 4 makes throwing from a move provider the *supported* way to abandon a game, so an
exception passing through `run()` is a normal shutdown path, not a crash path. Whoever calls
`run()` has to restore whatever they set up before it.

The terminal needs no work here, and it is worth recording why, because the reason does not
generalise: `Terminal`'s destructor calls `leaveAltScreen()` (`Terminal.cpp:119`, idempotent
against `altScreenActive`), and `main()` catches `std::exception` around the whole game
(`main.cpp:38-50`) so that destructor actually runs instead of being skipped by
`std::terminate`. The explicit `leaveAltScreen()` above is for the ordinary path — putting the
final scoreboard in the scrollback — and the destructor covers the throwing one.

**A Qt or web client has neither half.** Nothing in a slot or a request handler unwinds a
half-built UI by itself, and wake-and-throw is the *routine* way those clients end a game — a
closed window, a dropped session — rather than a rare failure. Phase 6 point 3 documents the
contract; the clients have to act on it. `QT_CLIENT_PLAN.md` is where the worker thread's catch
belongs.

### 3.8 Threading and blocking: the contract

`run()` blocks until the game is over. Every move provider and every observer is called on the
thread that called it, and `IMoveProvider` is where a game *waits for a human*. That is the
whole cost of inversion of control, and it has to be stated here rather than discovered by the
second client.

**The contract, in three lines:**

1. **One thread per engine, and no internal locking.** An engine is created, played and destroyed
   on a single thread. Do not add a mutex to `GameEngine` to make this softer — a client that
   needs concurrency owns the thread, not the engine.
2. **`IMoveProvider` is the suspend point.** A provider that waits on a human blocks the game
   thread for as long as the human takes. This is by design.
3. **A client whose UI is not on the game thread must snapshot inside the callback.** The
   `const GameEngine&` an observer receives must not escape to another thread — the engine has no
   thread safety at all, and a cross-thread read racing a deal is a torn read of a hand.
4. **Unparking a blocked provider is the client's job, and throwing is how it returns.**
   `requestStop()` only sets a flag read between tricks, so it cannot free a provider already
   parked on a human — and a window closing mid-turn is exactly when one is. The provider has no
   non-throwing way to say "abandon": every other return value is a move, and `std::nullopt`
   means "no legal play", which the engine treats as a bug. So a provider woken to abandon the
   game **throws**, which propagates out of `run()` and leaves the engine unresumable — the right
   outcome when the game is being discarded anyway. A GUI needs both halves: `requestStop()` for
   the idle case, wake-and-throw for the parked one.

Each planned client resolves this differently, and all three are compatible with the design above:

| Client | How it copes |
|---|---|
| Terminal | Nothing to do. Its "UI" is a blocking `std::cin`, so the game thread and the UI thread are the same thread, and the block *is* the prompt. |
| Qt | Engine on a worker thread; observer snapshots by value on the game thread and emits queued signals; the move provider parks on a condition variable until a click calls `submit()`. Spelled out in [QT_CLIENT_PLAN.md](QT_CLIENT_PLAN.md) §3-4. |
| Web | Thread per in-progress game, same shape as Qt: the provider parks until the move arrives over HTTP. Costs a parked thread per active game, which is acceptable at this scale — but it is a *choice*, and the timeout policy belongs to the backend, not the engine. |

**Why this does not paint us into a corner.** If a future client ever needs a non-blocking engine
— `playStep()` returning "waiting on seat N for a card", driven from an event loop with no thread
at all — that is an *additive* change, not a rewrite, **provided Phase 2 keeps the round's position
in engine members rather than in local variables of a nested loop.** The accessors in §3.3
(`getPhase`, `getActiveSeat`, `getCurrentTrickNumber`, `getCurrentTrick`) already require exactly
that, so the state machine is most of the way there for free. Phase 2 must not undo it — see the
guard there.

---

## 4. Two repos, one refactor

**This plan lives in the engine repo, but it cannot be executed in the engine repo alone.**

`romanian_whist_terminal` consumes this engine as a git submodule at `libs/RomanianWhistEngine`
(`https://github.com/conectionist/romanian_whist_engine.git`). It is currently the *only*
consumer, and it is also the only end-to-end check that the game still plays. Most phases here
change the engine's public API, so the terminal client stops compiling the moment they land.

### Which phases touch the terminal client

| Phase | Terminal work | Files |
|---|---|---|
| 0 — testability | **One narrow exception, otherwise none as planned.** `SetupWizard`'s custom-setup path let a spectator pick "1 bot" (no human seat), which `initializeDeck`'s new player-count guards turned from a degenerate game into a thrown exception; fixed by raising the minimum to 2 bots when spectating. Nothing else needed to change for the terminal to build and play. | `SetupWizard.cpp`, `engine-v4` branch, commit `949fee0` |
| 1 — seats | **Done.** Narrower than predicted: **two files**, since `ConsoleMoveProvider`, `Renderer`, `CardFormat`, `SetupWizard` and `Pacer` touch only `BetContext`/`PlayContext`/`Card`, none of which this phase changed. `Seat` is a struct with an explicit constructor (§3.1), so a seat built from a loop index is `Seat{i}` and an index taken from a seat is `seat.index` | `TerminalRomanianWhist.{h,cpp}` (every iterator use; `seatOf`, both `setResult` call sites and the `tricksWon` vector all deleted; `Player` access goes through `GameEngine::getPlayer(Seat)`; `trick.getPlayedCards()` → `trick.cardsInPlayOrder()` where a flat card list is wanted), `GameView.cpp` (`hasBet`/`getBet` by name → by seat, `getActual` → `getTricksWon`) |
| 2 — engine owns the loop | **Substantial rewrite** | `TerminalRomanianWhist.{h,cpp}` becomes observer callbacks; `markCurrentlyWinning` switches to `getCurrentTrickLeader()`; `GameView.cpp` loses `openingSeat` and rebuilds `view.table` from `getCurrentTrick()`'s seats; `Pacer` calls move into the callbacks. `startGame()` keeps its v3 setup sequence this phase — §3.7 shows the post-Phase-3 shape, Phase 2 step 9 the interim one |
| 3 — setup | Small | `applySetup()` → builds a `GameSetup`; `Renderer::drawGameOver` takes `std::vector<Standing>`; the scoreboard rows move off `getPlayerRoundScores()` onto `getRoundScore(Seat)` / `getTotalScore(Seat)` |
| 4 — cards by value | Moderate | `ConsoleMoveProvider.cpp` (`layOutHand`, index-returning `playCard`), `CardFormat.{h,cpp}`, `GameView.cpp` |
| 5 — Forehead/Hidden | Small but visible | `ConsoleMoveProvider` (blind bid prompt), `Renderer`/`GameView` (ask `canSeeHand()` per seat instead of deciding from `humanSeat`) |
| 6 — release | Submodule pin, README | — |

### How to sequence the commits

**A phase is not done when the engine compiles. It is done when the terminal plays a full game.**
The migration of the client is *part of* each phase, not a follow-up to it.

Recommended workflow — a long-lived `engine-v4` branch in the engine repo that the terminal's
submodule tracks for the duration:

```bash
# engine repo: do the phase on the shared v4 branch
cd RomanianWhist && git checkout engine-v4
# ... implement the phase, ctest green ...
git commit && git push origin engine-v4

# terminal repo: point the submodule at it, migrate, verify by actually playing
cd ../romanian_whist_terminal/libs/RomanianWhistEngine
git fetch && git checkout engine-v4 && git pull
cd ../.. && cmake --build build && ./build/terminal_romanian_whist
# ... migrate the client until it plays a full game ...
git add libs/RomanianWhistEngine src/ && git commit    # submodule SHA moves with the client
```

Only at Phase 6 does `engine-v4` merge to `master` and the submodule pin move to a master SHA.

> **Do not merge an engine phase to `master` before the matching terminal migration is written
> and verified.** Doing so leaves `master` in a state where the engine's only consumer does not
> build, and the refactor loses the one thing that proves it still works.

The planned web backend does not exist yet, so there is nothing to coordinate there — but it
should be built against v4 rather than against v3 with the pointer and ordering hazards worked
around. Starting it before Phase 3 lands means writing code twice.

---

## 5. Phases

Each phase ends with the full test suite green and the terminal client playable. **Do not
proceed to the next phase until both hold** — see §4 for what "playable" requires.

**Progress: Phases 0 and 1 done, engine and terminal both. Phases 2–6 not started — Phase 2 is next.**

### Phase 0 — Make the engine testable, then pin its behaviour [DONE]

Nothing else can be done safely first. There are no tests today.

**Status: done.** Merged to `master` as
[`a3f82ea`](https://github.com/conectionist/romanian_whist_engine/commit/a3f82ea) (squashed from
eight review-driven commits on `engine-v4`). `ctest` green on both presets, `-Wall -Wextra` clean,
terminal client rebuilt from scratch and still plays a full game — with the one exception recorded
in §4's table above.

0a–0g shipped as specified below. Several rounds of independent code review — not anticipated by
this plan, since none of it was known to be broken until someone went looking — found real bugs
the original checklist didn't cover. All were fixed and covered by regression tests before merge:

- **`Round::setResult` could be mistaken for a real bet.** `hasBet()` checked map membership, and
  `setResult()` (recording tricks won) inserted a map entry the same way `setBet()` does — so
  seeding a result before a player had bet made `getForbiddenBet()` silently stop enforcing the
  final-bidder restriction for the rest of the round. Reproduced against the built library before
  fixing. This bug predates this plan entirely; nothing above knew to look for it.
- **Setup and dealing are now fully guarded, not just at entry.** `initializeDeck` rejects a
  `playerCount` that doesn't match `players.size()` (confirmed under ASan as a heap-buffer-overflow
  otherwise) and rejects being called a second time outright — an earlier "clear and rebuild"
  attempt at the second-call case was itself found to dangle every already-dealt `Card*` under
  ASan, which is why it rejects rather than retries. `addPlayer` rejects a duplicate name
  (`Round::bets` is name-keyed, so a collision silently shares one bet and one trick count between
  two seats — confirmed by reproduction) and rejects being called after `initializeDeck()` or
  `initializeScoreboard()` has run, for the same reason those two reject a second call on
  themselves. `dealCards()` requires both to have completed first.
- **This lands ahead of, and changes the shape of, Phase 3's plans below.** "Reject a second
  `start()`" and "duplicate names are rejected" (§Phase 3) are *already true* today, as interim
  per-method guards rather than one `start()`-level check. Phase 3 still has real work — folding
  these into one validation pass, and adding the empty-name rejection Phase 3 specifies that
  nothing above implements yet — but it is consolidating existing behaviour, not inventing it.
  Re-read Phase 3's relevant sections (marked below) before implementing them.
- **Two smaller test-infrastructure fixes, worth knowing about when trusting this safety net.**
  CI's `ctest` invocations now pass `--no-tests=error`, since a build where `WHIST_BUILD_TESTS`
  lands off otherwise prints "No tests were found!!!" and exits 0 — all three jobs green, having
  run nothing. And the property suite's card-legality check was found to be tautological (it
  re-derived "legal" from the exact `CardValidator::getLegalCards` call the strategy under test had
  just used to choose the card, so it could not fail regardless of whether that function was
  correct); replaced with an independent reimplementation of the rule, confirmed to actually catch
  a mutation the old check passed straight through.

None of this moved a golden score — every fix above closes a path nothing in the terminal client
or the test harness ever exercised.

**0a. Two-byte `Card`** (small, orthogonal, and it settles a design question later)

Give both enums a fixed underlying type:

```cpp
enum class Rank : std::int8_t { Two, Three, /* ... */ Ace };
enum class Suit : std::int8_t { Hearts, Diamonds, Spades, Clubs };

static_assert(sizeof(Card) == 2, "Card is passed by value everywhere from Phase 4 on");
```

Measured on the dev machine (g++ 13.3, x86-64):

| | size | align |
|---|---|---|
| `Card` today | 8 | 4 |
| `Card*` | 8 | 8 |
| `Card` with `int8_t` enums | **2** | 1 |
| `std::optional<Card>` | 3 | 1 |
| `std::optional<Suit>` | 2 | 1 |

An 8-card hand — the largest a round ever deals — goes from 64 bytes of pointers plus 64 bytes
of cards elsewhere in the deck, to 16 contiguous bytes.

Why this belongs in Phase 0 rather than Phase 4:

- It is two annotations and a `static_assert`, independent of everything else.
- It removes the only real argument against Phase 4. At 8 bytes, `sizeof(Card) == sizeof(Card*)`,
  so "pass by value" is a wash and the churn is hard to justify. At 2 bytes it is strictly
  smaller *and* strictly safer, and Phase 4 stops being a judgement call.
- It settles an open question in Phase 4: `trump` becomes `std::optional<Card>` (3 bytes) rather
  than a pointer, and `PlayContext::leadSuit` becomes `std::optional<Suit>` (2 bytes) rather than
  `const Suit*` with a null convention.

It also hardens a real undefined-behaviour path. A scoped enum with no fixed underlying type has
a value range set by its enumerators — `Rank` holds 0–12, so its range is 0–15, and
`static_cast<Rank>(42)` is UB. With a fixed underlying type the range becomes the underlying
type's and that cast is an ordinary integral conversion. This matters because
`GameEngine.cpp:27` reads:

```cpp
for(int r = 1 + (6 - playerCount) * 2 ; r < 13 ; r++)
```

`playerCount` is unsigned, so 7 players underflows and the loop casts nonsense into `Rank`.
Today that is UB; afterwards it is merely garbage cards. It still needs the 2..6 validation from
Phase 3 — this only downgrades the failure, it does not fix it.

> `std::int8_t` is `signed char`, so a raw rank streamed to `std::ostream` would print as a
> control character. The codebase already casts to `int` at every comparison and print site, so
> there is no regression — just don't let a `std::cout << static_cast<std::int8_t>(rank)` appear
> later.

**Verify:** golden scores unchanged (for valid player counts the deck-building cast stays in
range, so they will not move).

**0b. Seedable shuffle** (prerequisite — golden tests are impossible without it)

- `Deck::shuffle(std::mt19937& generator)` replaces the internal `random_device`.
- `GameEngine` owns a `std::mt19937`, seeded from `std::random_device` by default.
- **Seeding alone does not make a game reproducible — write the shuffle by hand.** Of the pieces
  involved, only `std::mt19937` is specified by the standard. `std::shuffle` and
  `std::uniform_int_distribution` are not, and libstdc++, MSVC STL and libc++ all differ in how
  many values they draw and how they map them. `.github/workflows/ci.yml` builds on
  `ubuntu-latest`, `windows-latest` **and** `macos-latest`, so a golden test pinning exact final
  scores passes on one of the three and fails on the other two — with a diff that looks exactly
  like a logic regression and is not one. Implement Fisher-Yates directly against `generator()`
  inside `Deck::shuffle`, and draw with explicit rejection sampling rather than
  `uniform_int_distribution` anywhere a seeded stream has to be reproducible. It is a dozen lines,
  and it is the difference between goldens that work on the dev machine and goldens that work in
  CI. Pin it with a test that asserts a known seed yields a known deck order, so the day someone
  reaches for `std::shuffle` again it fails immediately rather than on the next platform.
- **The seed needs a way in, and `GameSetup` does not exist until Phase 3.** Phase 0's entire
  deliverable is the golden suite, and a golden game cannot be seeded through an API three phases
  away. So add an interim entry point now: an explicit `GameEngine(std::uint32_t seed)`
  constructor beside the default one. It is purely additive, so Phase 0 stays source-compatible
  for the terminal client (§4). Phase 3 folds it into `GameSetup::shuffleSeed` and deletes it;
  until then it is what every golden test calls.
- Add an optional seed to `RandomCardStrategy` (constructor taking `std::uint32_t`), defaulting
  to non-deterministic, with a member generator and the same hand-rolled draw as above — it has
  two `uniform_int_distribution` uses today (`RandomCardStrategy.cpp:21,42`) and they carry the
  same portability problem. It is deliberately kept out of the golden games (§0f) — a seeded random
  legal player earns its keep in the property tests below, where exploring many seeds is the
  point, not in tests that pin one exact score.
- **The deck has to become inspectable, or two of Phase 0's tests cannot be written at all.**
  `Deck` exposes only `addCard`, `shuffle` and `operator[]` (`Deck.h`) — no `size()`, no
  iteration — and `GameEngine::deck` is a private member with no accessor, so the only window
  into the deck today is `getCurrentTrumpCard()`. Both the seed-determinism test above and 0e's
  deck-composition row need to read the whole thing. Add `Deck::size()` and `begin()`/`end()`,
  plus a `const Deck& getDeck() const` on `GameEngine`. Prefer that to a test-only `friend`:
  deck size is a reasonable question for a client to ask, and a friend declaration would make
  the tests the only thing holding the accessor open. This is the first thing an implementer
  hits in Phase 0 and it is a few lines; it just has to happen before the tests, not after.

**0c. Reject impossible player counts** (moved forward from Phase 3)

`initializeDeck`'s `1 + (6 - playerCount) * 2` underflows for 7+ players and casts nonsense into
`Rank`. Phase 0a downgrades that from undefined behaviour to garbage cards; the actual fix is a
2..6 check, which costs two lines and depends on nothing. Do it now rather than three phases
later, so no window exists where the hole is merely narrowed.

Put it in `initializeDeck` for now (throwing `std::invalid_argument`) and move it into
`start()`'s validation in Phase 3, where it joins the name checks.

**0d. Test target**

- `WHIST_BUILD_TESTS` option, defaulting to `ON` only when `PROJECT_IS_TOP_LEVEL` — so the
  terminal client and the web backend never build the engine's tests.
- Catch2 v3 via `FetchContent`, pinned. Add `enable_testing()` + `ctest`.
- New directory `tests/`, added to `CMakeLists.txt` explicitly (the source list is not globbed).

**0e. Unit tests** — these encode rules and must survive every later phase unchanged:

| Area | Cases |
|---|---|
| `CardValidator::getLegalCards` | leading returns whole hand; must follow lead suit when held; must trump when void in lead and holding trump; free discard when void in both; overtrumping not required; empty hand → empty |
| `CardValidator::beats` | trump over plain; higher trump over lower; lead suit over off-suit; higher rank within suit; two off-suit discards → incumbent stays |
| Forbidden bet | empty for all but the final bidder; empty for the final bidder when bids already exceed the trick count; otherwise exactly `trickCount - total` |
| `isBetLegal` | rejects `> trickCount`; rejects the forbidden value; accepts everything else in range |
| Round scoring | exact bid → `5 + bid`; miss → `-abs(bid - actual)` |
| Streaks | 5 consecutive hits → `+10`; 5 consecutive misses → `-10`; 1-card rounds excluded |
| Scoreboard schedule | 1-8-1 and 8-1-8 shapes; round count `3N + 12` (+2 with `endWithForeheadAndHidden`); opening seat advances one per round; `all1GamesAreForehead` marks every 1-card round |
| Deck composition | **All five legal counts**, not just the even ones: 2p → Jack..Ace (16 cards); 3p → Nine..Ace (24); 4p → Seven..Ace (32); 5p → Five..Ace (40); 6p → Three..Ace (48). `Rank::Two` never dealt. 3 and 5 are the counts worth pinning: `1 + (6 - playerCount) * 2` is least obviously right at odd values, and the deal has no slack — an 8-card round hands out `8 × playerCount` cards, which is the whole deck exactly, and a shorter round takes its trump from index `trickCount × playerCount`. Assert the deck size and the lowest rank present for each count. Needs the `Deck` accessors added in 0b — the deck is unreachable from the public API today. |
| `canSeeHand` | Normal: own hand only; Forehead: every hand but your own; Hidden: none. Added with the method in Phase 5. |
| Setup validation | 1 seat and 7 seats both throw (Phase 0c). The name rules — duplicate and empty rejected, `"John"` / `"john"` accepted as distinct — and a second `start()` throwing arrive with `start()` in Phase 3; add those cases then. |
| Shuffle determinism | the same seed yields the same deck order, and two engines given one seed play an identical game. Pins Phase 0b's hand-rolled Fisher-Yates so a later edit reaching back for `std::shuffle` fails here rather than on another platform's CI runner. |

**0f. Golden full-game tests** — the safety net for the whole refactor:

- Fixed seed (via 0b's interim constructor), deterministic strategies only: `FirstCard`,
  `LowRisk`, `Ducking`.
  **Keep `RandomCardStrategy` out of the golden games.** Note this is a statement about the
  strategy *after* 0b. Today it reseeds from `std::random_device` on every call, so it has no
  stream at all and is simply unusable in a golden test. Once 0b gives it a seed and a member
  generator it becomes usable — and starts being *dangerous* instead: its draw count depends on
  what the other seats bid, so any behavioural change anywhere desynchronises the stream and
  every later round moves, which turns a golden failure into "something changed somewhere"
  rather than a located one. Phase 5 depends on this; see the note there.
- Run a full game for 2, 4 and 6 players, both structures.
- Assert the **exact final score of every seat**, plus per-round bid/actual for at least one game.
- These tests must drive the game through the *current* API in Phase 0 (duplicating the loop —
  unavoidable, and the last time it ever needs duplicating).
- **Read the recorded values through a fixture accessor, not through the engine API directly.**
  0e's unit tests encode rules and are meant to survive every later phase unchanged; the goldens
  cannot, because the API they read *through* is what this refactor is changing.
  `getPlayerScores()` dies in Phase 3, `getActual(name)` and `getBet(name)` in Phase 1, and by
  Phase 4 even the card type has moved. Written naively, every phase edits six test files, which
  is how a safety net stops being maintained.

  Record only two things, and put both behind one thin shim in the test fixture:
  `finalScores(engine) -> std::vector<int>` **indexed by seat**, and
  `roundRecord(engine) -> std::vector<std::vector<std::pair<unsigned int, unsigned int>>>` of
  `(bid, tricksWon)` per round per seat, accumulated at each round's end. The frozen values are
  plain integers keyed by position, so they never need re-recording; each phase updates the shim
  and nothing else. Note that keying by seat rather than by name is what makes the values stable
  across Phase 1 in the first place.
- **Put that duplicated loop in one shared test helper, not inline in each test.** Phase 2 swaps
  it for `run()` in a single place, and — for one commit — runs *both* and compares. Writing it
  inline in six tests makes that comparison too tedious to bother with, which is how the safety
  net gets skipped at the exact moment it matters most.

> **Record the golden values by inspection, not by blindly pasting whatever the code emits.**
> Verify by hand that at least one full round's bids, tricks and scores are actually correct
> before freezing them. A golden test that pins a bug is worse than no test.

**0g. Property tests** — where a seeded `RandomCardStrategy` belongs

Goldens prove "same as before" only for the handful of paths their seeds happen to walk, and
three deterministic strategies walk three narrow, highly correlated ones. A random legal player
explores the rest. Over a few thousand seeded games, assert the invariants:

- no player ever plays a card `CardValidator::getLegalCards` did not offer;
- no bid ever equals `getForbiddenBet()`, and no bid exceeds the trick count;
- tricks won across the seats always sum to the round's trick count.
  *(Phase 1 note: results are now counted off the round's stored tricks, so this sum only
  recounts them. The test asserts `getPlayedTrickCount() == getCurrentRoundTrickCount()`
  instead — that the round played out as many tricks as it was dealt for.)*

This is the net for exactly the class of bug Phase 2 can introduce and a golden cannot catch.

**Draw the "few thousand" seeds from a fixed range, not fresh every run.** `0..N` for a fixed `N`,
committed alongside the test. §0b's whole argument against `std::shuffle` is that an
unreproducible source of randomness turns a real failure into noise nobody can attribute; a
property test that reseeds from `std::random_device` on every CI run makes exactly that mistake
one layer up — a rare violation becomes a flaky failure that passes on rerun and cannot be
attached to the commit that introduced it. Fixing the range costs nothing (the point of a property
test is breadth across seeds, not which seeds), and it means a failure here reproduces locally
with the seed CI printed.

**Verify:** `ctest --output-on-failure` green; terminal client still plays.

---

### Phase 1 — Seats and the round data model [DONE]

Mechanical apart from step 6, which forces one design decision — no behaviour change either way.
The golden tests must produce byte-identical scores.

1. Add `Seat.h`. Convert the public API of `Round`, `Trick`, `Scoreboard` and `GameEngine` from
   `PlayerList::iterator` to `Seat`. Keep iterators inside `PlayerList`.
2. `Round::bets` becomes `std::vector<std::optional<unsigned int>>` indexed by seat.
   `getBet(Seat) -> std::optional<unsigned int>`.
3. **Delete `Bet::actual`, `Round::setResult`, `Round::getActual`, `Round::hasBet`.**
   Add `Round::getTricksWon(Seat)`, counting stored tricks by winner.
4. `Scoreboard::calculateScores` moves onto seats for **both** halves of its comparison, not
   just one. `getTricksWon(seat)` replaces `getActual(name)`, and `getBet(seat)` replaces
   `getBet(name)` — which now returns `std::optional`, so it needs dereferencing. Scoring only
   ever runs after betting is complete, so **assert the optional is engaged** rather than
   defaulting a missing bid to 0: a disengaged bet here means the loop skipped a bidder, and
   quietly scoring that as a bid of zero would bury it. Its `for(auto& player : players)` loop
   needs the seat index too — trivial once step 7 makes `PlayerList` a vector.
5. `GameEngine::getForbiddenBet()` counts over `bets` directly; delete the defensive
   walk-the-players comment, which no longer describes anything.
6. `Trick` stores `Seat winner` **and a `Seat` alongside every played card** — `playedCards`
   becomes `std::vector<PlayedCard>`. In this phase that is `PlayedCard { Seat seat; Card* card; }`;
   Phase 4 turns the member into a `Card` by value and §3.5 shows the finished shape. Add
   `getCardPlayedBy(Seat)`. `Round.h` and `Trick.h` drop their `PlayerList.h` include.
   `determineTrickWinner` reads the winner's seat off the winning entry and stops calling
   `players.advanceCircular(firstPlayer, bestIndex)`, which is the last place a played-card
   *position* was being translated back into a seat. Doing this here rather than in Phase 2 means
   the engine's own loop is written against seats from the first day it exists.

   **This step is where Phase 1 stops being purely mechanical, so decide it deliberately.**
   `Trick::getPlayedCards()` is fed straight into `PlayContext::playedCards`
   (`TerminalRomanianWhist.cpp:226`), which is a `const std::vector<Card*>&` consumed by
   `CardValidator::getWinningCard` and by `TrickHeuristics::safeCards` / `winningCards`. Changing
   the trick's storage to `std::vector<PlayedCard>` therefore breaks that pass-through and, with
   it, every strategy — unless something rebuilds a flat card list.

   **Rebuild it; do not put seats into `PlayContext`.** Phase 4's target for
   `PlayContext::playedCards` is a flat `std::vector<Card>` (§Phase 4), so pushing `PlayedCard`
   inward here would only have to be undone. The engine materialises the flat projection from the
   in-flight trick each time it asks a provider for a card. That costs one small vector per play,
   against a hand of at most eight cards, which is not a cost worth designing around — and Phase 4
   makes it a vector of 2-byte values.

   If a strategy ever needs to know *who* played what, the seats are on the trick and
   `PlayContext` grows a second field for them (appended, per §3.6). None of the four bundled
   strategies reads anything but the cards today.
7. **`PlayerList` switches from `std::list<Player>` to `std::vector<Player>`.** The list existed
   only for iterator stability; once `Seat` is the public currency that reason is gone, and
   `at(seat)` stops being an O(n) walk. `Player` is move-only (it holds a `unique_ptr`), which
   `std::vector` handles — give `Player` a `noexcept` move constructor, or `reserve()` the seat
   count once at setup, so reallocation never surprises anyone. Internal `next()` /
   `advanceCircular()` become index arithmetic and get simpler.
8. Update the terminal client mechanically: `seatOf()` still exists here as a local shim if
   needed. Three call sites go with the deleted methods, not one:
   - the result-seeding loop, `TerminalRomanianWhist.cpp:143-151` — deleted outright (see §2,
     it was already dead code);
   - the **per-trick** `game.setResult(winner, tricksWon[winnerSeat])`,
     `TerminalRomanianWhist.cpp:271` — deleted too, along with the `tricksWon` vector it feeds;
   - `GameView.cpp:52`, `seat.tricksWon = round.getActual(seat.name)` → `getTricksWon(seat)`.

> **This closes a const-correctness hole for free.** `Round::getFirstPlayer()` and
> `getOpeningPlayer()` currently hand out a **mutable** `PlayerList::iterator` from a `const
> Round` — their own header comments admit it — so `getCurrentRound()` does not in fact seal off
> the players behind it. A `Seat` is a value and carries no access, so the hole closes the moment
> they return one. Worth recording: it is a second argument for §3.1 beyond decoupling headers,
> and without it a reviewer may read the whole phase as cosmetic.

**Verify:** golden scores unchanged; terminal plays a full game.

#### What implementing it found, beyond the checklist above

The engine half landed as written and the golden scores did not move. Five things the
checklist did not account for:

- **`Round` had no way to size its bets vector.** Step 2 indexes `bets` by seat but the
  constructor never learned the table size. It now takes a `seatCount`, passed by
  `Scoreboard::initialize` from `players.size()`.
- **Step 1 closes the only mutable door into `Player`.** `getFirstPlayerOfTheRound()` and
  `getNextPlayer()` were the sole accessors handing out a non-const `Player&`, and that is how a
  caller reaches the non-const `playCard()`. A `Seat` carries no access — which is the point —
  but the client and the test harness still drive the loop until Phase 2. Bridged with
  `Player& GameEngine::getPlayer(Seat)`, commented as a Phase 2 casualty. **Phase 2 must delete
  it**, or the hole §3.1 closes reopens permanently.
- **Step 4's "assert the optional is engaged" is a `throw`, not an `assert`.** Phase 0 already
  had to fix a guard that compiled out of a release build (`d93e355`); an `assert` here would
  repeat that. It also broke three existing tests that bid for only one of two seats — see below.
- **`Trick::getCardPlayedBy(Seat)` was added and then removed again.** Step 6 asks for it, but
  §3.3's review note lists it as one of two accessors with no caller in this plan and says to
  leave it out until one exists. It went in, had no caller and no test, and the client migration
  turned out not to want it either — `GameView` rebuilds its table by iterating
  `getPlayedCards()`. Deleted; it can come back when a Qt bidding panel or a web payload asks.
  **Step 6's instruction to add it is superseded by §3.3.**
- **Step 8's "three call sites" in the terminal is four.** `GameView.cpp:50`'s
  `round.hasBet(seat.name)` goes with the others, folding into `getBet(seat).has_value()`.
  §4's table had this right; the step list did not.
- **`determineTrickWinner`'s leader argument becomes dead**, so it was dropped and the method
  made `const`. It now throws on a trick with no cards at all, where `advanceCircular(leader, 0)`
  used to return the leader. It still ranks a *partly played* trick, which
  `markCurrentlyWinning` depends on.

Test fallout the phase description treats as mechanical but is not:

- `ScoreboardTests` drove scoring through `setResult`. With results derived from stored tricks,
  every such test has to award real `Trick`s — a local `awardTricks()` helper — and every seat
  must bid, or `calculateScores` now throws. `"Scoreboard round scoring"` gave both seats a
  trick in a **one-trick** round, which derived counting cannot represent; it moved to a 2-trick
  round, with its asserted scores unchanged.
- `GameEngineTests`' `"setResult before any bet does not corrupt getForbiddenBet"` section was
  deleted: bets and results no longer share storage, so there is nothing left to corrupt.
- `tests/TestSupport.h` is gone — it existed only to provide `seatOf()`.
- **The terminal migration was verified by byte-diffing a whole rendered game**, not by eye. The
  client has no tests and §4 asks for "plays a full game", which over 26 rounds is not something
  a reviewer can actually check. Seeding the engine and swapping the demo's `RandomCardStrategy`
  for a deterministic bot makes `--demo --auto` reproducible; capturing that game before the
  migration (against the Phase 0 engine, which has both the old API and the seeded constructor)
  and after gives a 15,668-line diff that must be empty. It was. Mutating `getTricksWon` to read
  the neighbouring seat moves 5,770 lines, so the check is not vacuous. Both patches are
  reverted afterwards. **Worth repeating for Phase 2**, which rewrites far more of the client.
- `tests/TrickTests.cpp` is new. `Trick` gained real logic this phase and had no direct
  coverage: the goldens exercise `cardsInPlayOrder()` only incidentally, and **reversing its
  play order leaves all seven golden games passing** — no golden asserts mid-trick state, and
  the bundled strategies read the trick as a set. `determineTrickWinner`'s empty-trick throw and
  its partly-played ranking (what the terminal's `markCurrentlyWinning` needs) are pinned in
  `GameEngineTests` for the same reason: mutating it to name the winner by position rather than
  by seat fails the goldens too, but only the targeted test says where.

#### What review found after the phase landed

Three fixes on top of the above, all with the golden scores still unmoved:

- **`Round::addTrick` accepted a trick with no winner.** Results are derived from stored winners
  alone, so a caller that added before `setWinner` scored every seat 0 tricks in silence — the
  half of the model `setBet`'s bounds check and `calculateScores`' missing-bid throw already
  guarded. It now throws `std::logic_error` on an unwon trick and `std::out_of_range` on a winner
  off the table. Phase 2's in-engine loop inherits the guard.
- **The property test's round-scored assertion had been weakened into a tautology.** It was
  swapped to `getPlayedTrickCount() == getCurrentRoundTrickCount()`, which the harness cannot
  violate — one trick is added per iteration. Summing `getTricksWon()` across the seats is *not*
  a recount, because that method only counts tricks with a winner set: with the sum restored,
  deleting the harness's `setWinner` fails on seed 0, where the trick-count form passed 1.17M
  assertions. Both assertions are kept.
- **`Seat` became a struct** (§3.1), which was the alias's whole reason for existing put back.


---

### Phase 2 — The engine owns the loop [NOT STARTED]

The core of the refactor.

1. Add `IGameObserver.h`, `GamePhase`, observer registration, `GameStatus::Stopped` and
   `onGameStopped`.
2. Add `GameEngine::playRound()` and `run()`. Port the body from
   `TerminalRomanianWhist::loop()` + `playCurrentRoundTricks()` — **this is the reference
   implementation and it is known-correct; port it faithfully rather than rewriting from the
   rules.** Replace each `renderer.drawFrame(view)` with the matching observer callback and
   drop the `pacer.*` calls (pacing moves to the client's observer). The one place *not* to port
   faithfully is move validation, which the reference loop does not do at all — see below.
3. `Round` owns the in-flight `Trick`; the engine sets the lead suit and appends cards.
   `getCurrentTrick()` exposes it. **Two rules govern its lifecycle and they pull in opposite
   directions** — implement both, or the port breaks one thing or the other.

   *Append before the callback.* The completed trick must be in `Round::tricks` before
   `onTrickWon` fires. `getTricksWon()` is derived by counting stored tricks (§3.4), so an
   observer redrawing the scoreboard from that callback would otherwise read a count one short
   for the winner, every trick. The terminal gets this right today only by the order its loop
   happens to run in (`addTrickToCurrentRound` at line 263, render at 276); a tidy-up during the
   port could lose it silently.

   *But do not clear it there.* `getCurrentTrick()` must still return the finished trick
   **throughout** `onTrickWon`; reset it at `onTrickStarted` instead. The obvious reading of the
   rule above — append and clear in one step — breaks the trick-end screen. The terminal draws
   the completed table *inside* that callback, under "X wins the trick" and "Press Enter for the
   next trick" (`TerminalRomanianWhist.cpp:273-280`), with `getCurrentTrickLeader()` supplying
   the winner highlight over it. Today `view.table` survives because the client accumulates it
   itself and clears it at the *start* of the next trick (line 197); afterwards it is rebuilt
   from `getCurrentTrick()`, so an emptied trick renders a blank table under a message naming a
   winner. **No test catches this** — the goldens cover rules, not presentation (§6). Only
   playing a game does, which is why §4 insists on it.

   *And rebuild it from the seats the trick carries.* `view.table` needs a seat per card, and
   Phase 1's `Trick` supplies one on every entry (§3.5). **Do not reconstruct it as
   `(getTrickLeaderSeat() + i) % playerCount`** — that is the same turn-order rule the client is
   being relieved of, and here it is also simply wrong: `getTrickLeaderSeat()` has already moved
   to the winner by the time `onTrickWon` fires, so the formula rotates the whole table on the
   one screen the player stops to read. This is the second silent-presentation-bug in the same
   callback and it has the same detection story: play a game.
4. Add the live-state accessors: `getPhase`, `getActiveSeat`, `getRoundLeaderSeat`,
   `getTrickLeaderSeat`, `getBiddingOrder`, `getCurrentTrickNumber`, `getTricksWon`,
   `getCurrentTrickLeader`. (These are all `const`, and `getCurrentTrickLeader` has no const
   path to implement itself today: `cardBeats` is non-const only because it resolves
   `getCurrentTrumpCard()` to the non-const overload. Mark `cardBeats` const and it picks the
   const one. Phase 4 dissolves the problem when trump becomes a value, but Phase 2 needs it
   first.)

   **Build `getCurrentTrickLeader()` on `CardValidator::getWinningCard`, not on a second ranking
   loop.** That function already copes with a partly played trick — its header says so — and it
   is the same rule `determineTrickWinner` uses, which is the entire reason this accessor exists
   rather than leaving `markCurrentlyWinning()` to reimplement one. The one wrinkle is that it
   returns the winning *card*, not a seat: scan the in-flight trick's `PlayedCard` entries for the
   one holding it, which Phase 1's per-card seats make a lookup rather than arithmetic. Note that
   `getWinningCard` still takes `std::vector<Card*>` at this point — Phase 4 moves it to values.

   **Add the `requireStarted()` guard in the same commit as the accessors** (§3.3). Every one of
   these reads the current round, `Scoreboard::getCurrentRound()` indexes an empty vector before
   setup, and the whole point of publishing them is that clients with a setup screen will call
   them early. Retrofitting the guard later means auditing every accessor instead of writing one.
5. `requestStop()` sets an **atomic** flag, checked at the top of each trick and each round; it is
   the one method callable from another thread. Reaching the end of the schedule gives
   `GameStatus::Finished` + `onGameOver`; an honoured stop gives `Stopped` + `onGameStopped`.
   A stopped engine is not resumable.

   **Add a dedicated test for this, in the same commit.** Nothing else in the plan exercises it —
   0e predates `requestStop()`, 0g's property games run to completion, and Phase 2's own
   manual-play check exercises pacing, not cancellation. The boundary-checked flag and the
   `Playing`-phase-left-unscored behaviour are exactly the kind of thing a tidy-up could shift by
   one trick without anything else noticing. Cover at least: `requestStop()` called before `run()`
   starts leaves the schedule untouched and fires `onGameStopped` with zero rounds played;
   `requestStop()` called mid-trick lets that trick finish (checked at the boundary, not
   mid-turn) and stops before the next one, with `getPhase() == Playing` and the round unscored;
   and a stopped engine's `run()`/`playRound()` reject a second call the same way a finished one
   does (§3.3's status guard).
6. Make `Player::playCard` / `Player::getBet` private, `friend class GameEngine`.
7. **Prove the port before deleting the reference.** Add `run()` while the old driving methods are
   *still public*, and add one test that plays the same seed both ways — through the Phase 0
   duplicated loop and through `run()` — asserting identical final scores and identical per-round
   bids and tricks. See below.
8. Only then: make the former driving methods private, switch the remaining golden tests to
   `run()`, and delete the duplicated loop helper. **Scores must not change.**
9. Rewrite the terminal client as observer callbacks (§3.7). **§3.7 shows the post-Phase-3 shape,
   so `startGame()` is not yet what is written there.** `GameSetup`, `start()` and
   `getStandings()` all arrive in Phase 3. Until then the client keeps its existing setup
   sequence and only the driving call changes:

   ```cpp
   void TerminalRomanianWhist::startGame()
   {
       applySetup(...);                       // addPlayer / initializeScoreboard / initializeDeck,
       game.addObserver(this);                // unchanged from v3

       game.setStatus(GameStatus::InProgress); // still public until Phase 3
       game.run();

       terminal.leaveAltScreen();
       renderer.drawGameOver(game.getPlayerScores());   // Standing arrives in Phase 3
   }
   ```

   Every observer callback in §3.7 *is* final from this phase on; the churn is confined to these
   few lines, which is the reason this phase and Phase 3 are worth keeping separate rather than
   pulling `start()` forward into the largest phase in the plan.

   The one thing to add for the interim: **`run()` and `playRound()` throw `std::logic_error`
   unless the status is `InProgress`.** For this one phase the engine owns the loop while the
   client is still responsible for remembering to start it — exactly the load-bearing, unenforced
   ordering §1 objects to — and without the check a client that forgets gets a `run()` that
   silently returns having played nothing. Phase 3 makes the situation unreachable by folding the
   transition into `start()`; the check stays anyway, since it is then what rejects a second
   `run()` on a finished or stopped engine.

#### The engine validates moves, now that it is the one asking for them

Legality lives entirely inside the providers today. `AiMoveProvider` delegates to a strategy that
calls `CardValidator::getLegalCards`; `ConsoleMoveProvider` calls it directly
(`ConsoleMoveProvider.cpp:114`). `GameEngine::placeBet` records whatever it is handed "without
judging it" (its own header comment), and *nothing anywhere* checks a played card against the legal
set. That is defensible while the only client is a local terminal whose provider ships in the same
repo as the rules.

It stops being defensible at the first `WebMoveProvider`, which is a thin shim over an untrusted
browser — and a web backend is a stated motivation for this whole refactor (§1). An index arriving
over HTTP would reach `Round` unchecked. §1's argument applies here with more force than anywhere
else in the plan: a rule the client is trusted to enforce is a rule an adversarial client simply
does not enforce.

Note what this does to Phase 0g. "No player ever plays a card `getLegalCards` did not offer" and
"no bid ever equals `getForbiddenBet()`" are written there as invariants, but without engine-side
checking they are properties of *the four bundled strategies*, not guarantees of the engine — and
the property suite would stay green on an engine that cheerfully records an illegal move.

So, inside `playRound()`:

- a bid failing `isBetLegal()` throws `std::logic_error` before it reaches `placeBet`;
- a card index outside the hand, or naming a card `CardValidator::getLegalCards` did not offer,
  throws the same. Phase 4's range check is the bounds half of this; the legality half is needed
  either way and can be written here against `Card*`.

**Checked: none of the four bundled strategies can trip either throw**, so adding validation here
does not move a single golden score. Worth recording, because "Phases 0-4 leave the goldens
byte-identical" (§7) depends on it and it is not self-evident. `FirstCard` and `Ducking` bid
`forbiddenBet == 0 ? 1 : 0`, and a round always has at least one trick.
`LowRiskStrategy::getBestBet` is bounded above by `std::min(winners, hand.size())` inside
`countLikelyWinners`, and its step-down (`bet == 0 ? 1 : bet - 1`) cannot land back on the barred
value. `RandomCardStrategy` draws over a range one short and steps over the barred value, so it
cannot produce it either, and its maximum is `hand.size()`. On the card side, every strategy
selects from a `getLegalCards` result — directly, or through `TrickHeuristics`, whose
`safeCards` / `winningCards` are subsets of it — and `ConsoleMoveProvider` only ever offers the
human a numbered *legal* card (`ConsoleMoveProvider.cpp:50-56`). If a later strategy does trip a
throw, that is the check doing its job, not a regression in the check.

Throwing takes the not-resumable path above, which is the right outcome for a provider that is
simply wrong. A backend wanting to re-prompt a human validates *before* returning — `PlayContext`
carries the hand, trump and lead suit, and `CardValidator::getLegalCards` turns them into the legal
set — and treats a throw as the bug it is. If an in-engine re-ask hook is ever wanted it is
additive, so it need not be decided now. Leaving the move unchecked is not something that can be
deferred to the client that needs it most.

#### Why steps 7 and 8 are separate commits

The Phase 0 golden tests are the safety net for this phase, and the naive sequence rewrites the
net and the thing it guards in the same commit. If a golden value moves you learn only that
*something* changed, not whether the loop was ported wrong or the test was rewritten wrong.

Splitting it gives one commit where both paths exist and must agree — which is a far stronger
statement than "the new path still produces the number we wrote down." The old path is deleted in
the next commit, once it has done its job.

> **Guard against double scoring.** `Scoreboard::calculateScores` accumulates into
> `currentRoundScore` via `addToScore`, with no guard — calling it twice doubles the round's
> points. Now that the engine calls it, add a phase check so a future edit cannot introduce a
> second call silently.
>
> **Make it a phase check, not an assertion on the score**, because the doubled points are the
> *loud* half of the damage. The same second call double-increments the streak counters, so a
> player on their fifth consecutive hit steps 4 → 6 and the `== 5` bonus never fires at all — a
> missing ±10 that reads as a scoring-rule bug rather than a double call, and that no assertion
> about round scores would notice. Gate the whole function on the phase moving
> `Playing → RoundScored`, not just its arithmetic.

> **Keep the round's position in members, not in loop locals.** The obvious port is two nested
> `for` loops with `leader`, `currentPlayer`, `trickIndex` and the trick itself as locals. Do not
> do that. Those live in the engine (`Round` owns the in-flight trick; the engine owns the phase,
> the active seat and the trick number) and the loops merely *advance* them — which is what the
> §3.3 accessors already require, and what keeps a future non-blocking `playStep()` additive
> rather than a rewrite (§3.8). The loops stay; only the state moves out of them.

#### Failure and cancellation, decided here

These are Phase 2 design decisions because Phase 2 is where the engine starts owning a half-played
trick. Phase 6 only writes them down.

- **A provider or observer that throws** propagates out of `run()` / `playRound()`. The engine is
  left mid-round and is **not resumable** — the caller destroys it. No attempt is made to unwind
  to a round boundary; the state is already inconsistent and pretending otherwise is worse.
- **`requestStop()`** is the clean path for an engine between tricks. It stops at a trick
  boundary with the round unscored.
- **On its own it is not a shutdown.** The flag is read between tricks and rounds, so it cannot
  reach a move provider already parked on a human — and a GUI closing its window is precisely
  that case. The provider has no non-throwing way to say "abandon": every other return value is
  a move, and `std::nullopt` means "no legal play", which the engine treats as a bug. So **the
  supported shutdown for a parked provider is to wake it and let it throw**, taking the
  not-resumable path above — correct, since the game is being discarded anyway. Clients need
  both halves; §3.8 point 4 states the contract and QT_CLIENT_PLAN.md is where it gets built.
- Nothing here is exception-safe in the strong sense, and it does not need to be. Say so, so a
  future reader does not assume a guarantee that was never designed for.

**Verify:** golden scores unchanged; the both-paths-agree test passes before step 8 lands; the
`requestStop()` tests from step 5 pass; terminal plays a full game, with pacing and
Enter-to-continue behaving as before.

---

### Phase 3 — Setup consolidation [NOT STARTED]

1. Add `GameSetup`, `SeatSetup`, `GameEngine::start(GameSetup)`. Fold Phase 0b's interim
   `GameEngine(std::uint32_t seed)` constructor into `GameSetup::shuffleSeed` and delete it,
   repointing the golden tests at `start()` as you go — the last of Phase 0's scaffolding to come
   down. `GameSetup` holds `unique_ptr<IMoveProvider>` and is therefore move-only, and `start()`
   takes it by value, so every call site reads `game.start(std::move(setup))`.
2. Validate inside `start()`: names non-empty and unique, and **move the 2..6 seat check here**
   from `initializeDeck`, where Phase 0c put it. All setup validation now lives in one place and
   throws `std::invalid_argument`.

   **Reject a second `start()` as well**, with `std::logic_error`, since it is a state error rather
   than a bad argument. **Phase 0 already gets most of the way here** (see its status note above):
   `initializeDeck` and `initializeScoreboard` each now reject being called a second time, and
   `addPlayer` rejects being called after either has run, so most ways of misusing the current
   per-method API already throw before they can corrupt anything — the double-sized-deck failure
   mode this paragraph originally warned about (`initializeDeck` appending unconditionally) no
   longer exists. What Phase 3 still owns: `start()` folds the three interim guards into one call,
   so a second `start()` throws a single clear error naming `start()` itself, rather than whichever
   of the three internal methods happens to run first and throw about *itself*. Add it to the 0e
   setup-validation cases alongside the 1-seat and 7-seat rows.
3. Perform the four setup steps internally, in order. `initializeDeck` no longer takes a player
   count; it reads its own.
4. Delete `addPlayer`, `initializeScoreboard`, `initializeDeck`, `setStatus` from the public API.
5. `getStandings()` returns `std::vector<Standing>` carrying the seat, so duplicate names remain
   distinguishable. Sort it with `std::stable_sort`, not `std::sort` — the current
   `getPlayerScores` (`Scoreboard.cpp:195`) leaves tied players in unspecified order, so a tie
   can render differently between runs and any test asserting `getStandings()[0]` would flake.
   Stable makes a tie fall back to seat order, for free. Delete `getPlayerScores()` — the terminal's `drawGameOver` takes
   `std::vector<Standing>` instead. It is a breaking release; a deprecated shim buys nothing.
6. **`getPlayerRoundScores()` goes the same way.** It returns
   `vector<pair<string, pair<int, int>>>`, which is unreadable at the call site and keyed by name
   like everything Phase 1 just moved off names. Replace it with the seat-indexed
   `getRoundScore(Seat)` / `getTotalScore(Seat)` already listed in §3.3, and update the terminal's
   scoreboard rendering to ask per seat. Note that the old function's second element is
   `total + currentRound` (the *projected* total, not the committed one) — preserve that
   distinction wherever it is displayed, or the scoreboard will read differently between
   `onRoundScored` and `onRoundComplete`.

#### Duplicate names are rejected — decided

**Phase 0 already added this as an interim guard on `addPlayer`** (see its status note above),
with the same exact-byte-comparison reasoning laid out below — this section's decisions are what
that guard followed, not the other way round. What is *not* yet done: empty names are still
accepted, since a lone empty name is not a duplicate of anything. When folding the check into
`start()`, add the empty-name rejection in the same pass rather than leaving it as a second gap
someone has to rediscover.

`start()` throws `std::invalid_argument` if two seats share a name. Two players called John is
not a thing that happens at a real table; one of them becomes Johnny.

Keep the rule minimal, because every extra sophistication here is a bug waiting to happen:

- **Exact byte comparison. No case folding.** Tempting to reject "John" and "john" as the same,
  but this is a Romanian card game and the names will contain `Ș`, `Ț`, `Ă`, `Â`, `Î`. A naive
  byte-wise `std::tolower` over UTF-8 either does nothing to a multibyte sequence or corrupts it,
  and passing a negative `char` to `std::tolower` is undefined behaviour outright. Correct Unicode
  case folding is not something the engine should grow a dependency for.
- **Reject empty names** too, while validating.
- **No trimming.** Validate, don't mutate — a caller that passes `"John "` and reads back `"John"`
  from `getName()` has been surprised for no good reason.
- **Clients may be stricter.** `SetupWizard` already rejects duplicates case-insensitively via
  `Input::equalIgnoringCase`, which is fine for the ASCII bot names it generates. That behaviour
  is unchanged and needs no work.

> **This does not make Phase 1's seat-indexing redundant — it is the other way round.** Because
> bets are keyed by `Seat`, duplicate names are merely undesirable rather than corrupting, which
> is what lets this be a cheap policy check in one place instead of an invariant the whole data
> model has to defend. Do not read "we reject duplicates now" as licence to key anything by name
> again.

**Checked for fallout, none found:** `SetupWizard::demoConfig()` uses four distinct names; the
quick-game path already skips bot names that collide with the human's; the golden tests use
distinct names. No client changes are needed.

**Verify:** golden scores unchanged; 1-seat and 7-seat setups throw; terminal plays.

---

### Phase 4 — Cards by value [NOT STARTED]

Removes the two subtlest rules in the codebase, and fixes one live bug. No other phase depends on
it, but "optional" overstates that — see the third bullet.

**The problems it removes:**
- `Player::playCard` erases the played card via `std::remove` on `Card*`, so a provider that
  returns a copy silently erases nothing and the hand drifts.
- `Deck::shuffle` permutes values between slots, so any `Card*` cached across a shuffle stays
  valid but silently denotes a different card.
- **The engine does exactly that to itself.** `Scoreboard` retains every `Round`, and each one
  holds its trump and its tricks as `Card*` into the single `deck` that is re-shuffled at the top
  of the next round — so from round two on, every completed round misreports what was played in
  it (§2, third finding). This is not a hazard awaiting a careless client; it is wrong now. It is
  invisible only because no client reads history yet, and it is a blocker for the first one that
  does.

**The change:** make `Player::hand`, `PlayedCard::card` (the member inside `Trick::playedCards`,
which Phase 1 introduced holding a `Card*`), `PlayContext::hand`, `PlayContext::playedCards` and
`BetContext::hand` hold `Card` by value. After Phase 0a a `Card`
is 2 bytes against a pointer's 8, so this is smaller as well as safer. `trump` becomes
`std::optional<Card>` (3 bytes) and `PlayContext::leadSuit` becomes `std::optional<Suit>`
(2 bytes) — both replacing a pointer whose null meant "none", which is what an optional says
outright.

Add a defaulted `operator==` to `Card` while here.

#### The boundary: `playCard` returns an index

```cpp
// Returns an index into context.hand, or std::nullopt for "no legal play".
virtual std::optional<std::size_t> playCard(const PlayContext& context) = 0;
```

The engine range-checks the index and erases by position.

The alternative — returning `std::optional<Card>` and having the engine find-and-erase by value —
is legitimate and would work, because every card is unique within a single deck, so value equality
*is* identity here. Three reasons to prefer the index anyway:

1. **A fabricated card becomes impossible to express**, rather than detected and thrown. That is a
   different quality of guarantee, and it is the whole point of the phase.
2. **It is the only boundary a hostile caller cannot lie across.** A value has to be checked for
   membership; an index only has to be checked for range, and the engine already knows the hand.
   That matters most at the `WebMoveProvider` the Phase 2 validation section is written for.
3. **It keeps the erase honest.** Erasing by position is unambiguous; erasing by value relies on
   cards being unique within a deck, which is true today and is exactly the kind of quiet
   invariant this refactor exists to stop depending on.

**Two arguments that were made for the index and do not hold — do not repeat them.** An earlier
draft claimed the wire protocol is index-based anyway, and that
`ConsoleMoveProvider::layOutHand` already builds this mapping so the function would get shorter.
Both are wrong in the same way. `layOutHand` returns `choices`: the **legal subset, in display
order**, and the human's number indexes into *that*, not into `context.hand`
(`ConsoleMoveProvider.cpp:25-63`, and line 143 where the
number is turned back into a card). A browser is shown the same sorted, filtered list and can
only send back an index into it. So an index into `context.hand` does not remove a translation
step — it **adds** one to every provider that shows a reordered or filtered hand, which is both
of the interactive ones. `layOutHand` gets slightly longer, not shorter: it has to carry the
hand position alongside each choice.

That cost is worth paying for reason 1, which is the whole point of the phase. It is not worth
pretending the cost is not there — an implementer who reads a claim about `layOutHand` and then
opens `layOutHand` will start doubting the conclusion along with the argument.

#### Values inside, index at the boundary

The honest cost of the index is that positions are clumsier than cards *inside* a strategy, which
wants to reason about what a card is, not where it sits. So do not push indices inward:

- `TrickHeuristics` (`mostDangerous`, `leastDangerous`, `chooseDuckingCard`, `chooseWinningCard`)
  and all four `IStrategy` implementations keep working in **values**, returning
  `std::optional<Card>`.
- `CardValidator::getLegalCards` returns `std::vector<Card>` — values, matching the above — and
  takes `std::optional<Card> trump` and `std::optional<Suit> leadSuit` in place of its two
  pointers. Note its `trump` parameter is `Card*` today, non-const for no reason; the optional
  settles that too.
- **`CardValidator::getWinningCard` moves with them, and it is easy to miss.** It is
  `Card* getWinningCard(const std::vector<Card*>&, Suit, const Card*)` and becomes
  `std::optional<Card> getWinningCard(const std::vector<Card>&, Suit, std::optional<Card>)`. Two
  things depend on it: `TrickHeuristics::safeCards` / `winningCards`, which every strategy reaches
  through, and `GameEngine::getCurrentTrickLeader()` from Phase 2 step 4. It appears in none of
  the earlier drafts' change lists.
- **`AiMoveProvider` is the only bridge.** It takes the `Card` its strategy chose, finds it in
  `context.hand`, and returns that position — throwing if it is somehow absent, which would mean a
  strategy invented a card:

```cpp
std::optional<std::size_t> AiMoveProvider::playCard(const PlayContext& context)
{
    const std::optional<Card> chosen = strategy->getBestChoice(context);
    if(!chosen)
        return std::nullopt;

    const auto it = std::find(context.hand.begin(), context.hand.end(), *chosen);
    if(it == context.hand.end())
        throw std::logic_error("strategy chose a card that is not in the hand");

    return static_cast<std::size_t>(std::distance(context.hand.begin(), it));
}
```

Three lines of bridging, in one file, once — and every strategy stays readable.
`ConsoleMoveProvider` and any future `WebMoveProvider` return the index natively, since both
already think in terms of a numbered hand.

**Also update:** `Player`, `Trick`, `Round`, `CardValidator` (both functions above), the flat
`PlayContext::playedCards` projection Phase 1 step 6 introduced, and the `GameEngine` trick loop.

**Verify:** golden scores unchanged — this phase must not alter play at all. What it *does* alter
is what a finished round reports about itself, which goes from wrong to right: any assertion over
round history has to be written after this phase, never before it.

---

### Phase 5 — Forehead and Hidden [NOT STARTED]

Closes the gap `IMPLEMENTATION_PLAN.md` records. See §3.6 for why this is far smaller than it
first looks, and for what it deliberately does not do.

1. Add `RoundType roundType` to `BetContext` — **at the end of the struct** (§3.6).
2. Add `GameEngine::canSeeHand(Seat viewer, Seat holder)` (§3.6) and unit-test it: three round
   types x self/other.
3. `LowRiskStrategy::getBestBet` returns 0 when `roundType` is `Forehead` or `Hidden` rather
   than reading its hand — keeping the existing forbidden-bid guard, which already steps 0 up to
   1 when 0 is barred. **This is the only strategy that changes.** `FirstCard` and `Ducking`
   never read the hand to bid; `RandomCard` already draws uniformly over `{0, 1}` in a one-card
   round, blind or not.
4. `ConsoleMoveProvider` renders the blind bid prompt. `Renderer` / `GameView` ask
   `canSeeHand()` per seat instead of deciding from `humanSeat`.

#### This is the one phase that changes play — say so

Every other phase leaves the golden scores byte-identical. This one does not, and there is no
flag to pretend otherwise: an earlier draft carried an `enforceRoundTypes` toggle so the old
goldens could keep passing, but "v3 behaviour" is a game played wrong on purpose and there is no
reason to keep it reachable once it has served its purpose. The toggle is dropped.

So the goldens are **re-recorded here**, by the Phase 0 rule — by inspection, not by pasting
output. Two things keep that honest:

- **The diff must be confined to one-card rounds**, since those are the only Forehead and Hidden
  rounds the schedule ever produces. A Normal round whose bids moved means something leaked. Two
  facts make that a tight check rather than a hopeful one: `shouldCountForStreaks` excludes
  one-card rounds, so a moved bid there cannot leak a ±10 bonus into some later round's total; and
  a one-card round has no card decision at all, so the *only* things that can move are the recorded
  bid and the round's score. Anything else in the diff is a bug, not a consequence.
  Size the eyeballing correctly, though, and note it differs by structure. Under
  `all1GamesAreForehead` the affected rounds are `2N + 2` in **S_181**, which puts a block of N
  one-card rounds at each end (`Scoreboard.cpp:22-37`), but only `N + 2` in **8-1-8**, which has a
  single such block in the middle (`Scoreboard.cpp:41-55`). `endWithForeheadAndHidden` adds the
  final two in both. Since 0f records goldens for both structures, the two diffs are different
  sizes and a reviewer expecting `2N + 2` in the 8-1-8 game will go looking for rounds that were
  never affected.
- **Record the goldens with the deterministic strategies only** (§Phase 0f). This is what makes
  the check above mechanical rather than hopeful: `LowRiskStrategy` bidding 0 instead of 1
  changes the round's running total, which can flip whether `getForbiddenBet()` returns a value
  at all for the final bidder — which changes `RandomCardStrategy`'s draw count and
  desynchronises its generator for every round after. With no RNG strategy in the golden games,
  the difference stays where it belongs and the eye can check it.

### Phase 6 — Documentation and release [NOT STARTED]

Nothing is *decided* here — the contracts were settled in §3.8 and Phase 2. This phase writes
them where a client author will find them.

1. `README.md`: replace the "the engine does not own the loop — your client does" section with
   the observer model. Document that observers are called synchronously, that blocking pauses
   the game, and that observers must outlive the engine.
2. Reproduce §3.8 in the README: one thread per engine, **no internal locking**, `IMoveProvider`
   is the suspend point, and a `const GameEngine&` handed to an observer must not escape the game
   thread. Link the Qt client as the worked example of a client whose UI is elsewhere.
3. Document the cancellation and failure contract from Phase 2: `requestStop()` ends cleanly at a
   trick boundary and yields `onGameStopped`; an exception from a provider or observer propagates
   out of `run()` and leaves the engine **not resumable** — destroy it.
4. **Write the hand-visibility note for client authors.** `canSeeHand()` is a rule, not a
   guard: the engine will hand any caller any seat's cards, including the one that seat is not
   allowed to see. Say so where a backend author will read it, with the consequence spelled out
   — a server that builds its per-player payload from `getPlayers().at(n).getHand()` will
   serialize a player their own Forehead card, and hiding it in the browser is not hiding it.
   Build the payload through `canSeeHand()` so a card cannot reach the wire any other way, the
   same "make it inexpressible" argument Phase 4 makes for the index.
5. `IMPLEMENTATION_PLAN.md`: mark the round-type and test gaps closed.
6. Version to **4.0.0**. Every change here is breaking.
7. Merge `engine-v4` to `master` and move the terminal client's submodule pin onto a master SHA
   (§4). Update its README.

---

## 6. Risks

**Inversion of control.** Clients no longer drive. In practice this is mild — observers are
called synchronously on the same thread, so a breakpoint in `onCardPlayed` behaves exactly like
one in today's loop.

**A blocking observer stalls the game.** Intentional (it is the pacing hook), but it must be
documented, and a future web backend's observer must not block on network I/O.

**`IMoveProvider` blocks the game thread, and that is the load-bearing consequence of the
refactor** — not the observer. A human's provider parks until they answer, so every GUI and web
client needs a thread per in-progress game and a way to unpark a provider on shutdown — which is
a wake-and-throw, since a provider has no non-throwing way to abandon a turn. §3.8 point 4 states
the contract and [QT_CLIENT_PLAN.md](QT_CLIENT_PLAN.md) works it through. The mitigation against
being wrong about this is Phase 2's rule that round state lives in engine members: it keeps a
non-blocking `playStep()` an additive change rather than a rewrite. Do not let that rule slip.

**The golden suite is only as portable as the shuffle underneath it.** CI builds on Linux, Windows
and macOS, and `std::shuffle` / `std::uniform_int_distribution` are unspecified across those three
standard libraries. Goldens that pin exact scores are worthless — worse, actively misleading —
unless Phase 0b's hand-rolled Fisher-Yates lands with them. It is the one thing in Phase 0 that
fails on somebody else's machine rather than yours, which is the hardest kind of failure to
attribute after the fact.

**Owning the loop without validating moves would be half a refactor.** Phase 2 adds bid and card
checking because the engine is now the one asking; skipping it leaves the first untrusted client —
the web backend this refactor is largely for — free to bid illegally and play cards it does not
hold, with the Phase 0g property suite green throughout, since those invariants would still be
properties of the bundled strategies rather than of the engine.

**The terminal client is the only end-to-end check for pacing and rendering.** The golden tests
cover rules, not presentation. Play a full game by hand at the end of Phases 2 and 5.

**The refactor spans two repositories** and the client cannot build against a partially migrated
engine — see §4. The most likely way to get into trouble here is landing an engine phase on
`master` before the terminal migration that goes with it, which strands the only consumer.

**Round-type blinding is an honour-system rule, and the first multiplayer client is where that
bites.** `canSeeHand()` puts the rule in one place, but `getHand()` stays public because the
renderer needs it. The failure mode is not a cheating strategy — it is a web backend that builds
each player's payload straight from the engine and ships them their own hidden card, which any
viewer then reads out of devtools. It belongs in that backend's serialization layer and in a
test that asserts what is absent from each payload. See §3.6 and Phase 6.

**Phase 4 is a wide, shallow change** touching every strategy and both move providers. It is
also the only phase with a hard guarantee attached: scores must not move at all. If they do,
something in the port is wrong.

**Retained rounds misreport themselves until Phase 4.** Every finished `Round` holds its trump
and its tricks as pointers into a deck that is re-shuffled each round, so their contents change
underneath them (§2, third finding). Nothing reads history today. The first client that renders
"the previous round" will, and it will look like a rendering bug in that client rather than a
five-year-old aliasing bug in the engine.

**Scope discipline.** Phases 0–3 are the ones that pay for themselves immediately. **Phase 4 is
deferrable only for a client that never shows round history** — it is a correctness fix for the
retained-`Round` bug above, not merely a hardening pass, so a web or Qt client that wants a
history view needs it landed first. Phase 5 is genuinely separable: it is a feature, not a
refactor, and could ship on its own.

---

## 7. Order of work, condensed

```
0. 2-byte Card -> portable seeded shuffle -> 2..6 check -> tests -> goldens  [no behaviour change] [DONE]
1. Seat + per-card seats in Trick; results derived; PlayerList -> vector     [no behaviour change]
2. IGameObserver; playRound()/run(); engine owns + validates play; terminal  [no behaviour change]
   2a. run() beside the old loop, both paths asserted equal
   2b. old driving API goes private; duplicated loop deleted
3. GameSetup + start(); validation consolidated; getStandings()              [no behaviour change]
4. cards by value; playCard returns an index; fixes retained-round aliasing  [no behaviour change]
5. Forehead/Hidden: BetContext.roundType, canSeeHand(), one strategy         [changes play]
6. docs, version 4.0.0, submodule pin
```

**Phases 0-4 leave the golden scores byte-identical**, and a moved score in any of them means
the refactor broke something. **Phase 5 is the one exception**, deliberately: it is the phase
that changes how the game is played, so its goldens are re-recorded — by inspection, and with
the difference confined to one-card rounds. That is the only re-baselining step in this plan,
and it is in the only place a behaviour change was ever intended.

---

## 8. Reference files

- `src/GameEngine.cpp` — the primitives being consolidated; `getForbiddenBet()` (lines 96-124)
  explains the `setBet`/`setResult` coupling that Phase 1 removes.
- `src/Round.cpp` — `getBet`/`getActual`/`hasBet` (lines 79-102); the zero-vs-absent ambiguity.
- `src/Scoreboard.cpp` — `calculateScores` (114-156) reads by name and accumulates without a
  guard; `initialize` (13-82) is the round schedule the tests pin; `rounds` is append-only and
  `getCurrentRound()` (94-102) indexes it unguarded — the two facts behind §2's third finding and
  §3.3's pre-`start()` rule.
- `src/Player.cpp` — `playCard` (37-47) is the pointer-identity erase that Phase 4 removes.
- `include/romanian_whist/Card.h` — the two enums that gain `: std::int8_t` in Phase 0a.
- `src/Deck.cpp` — `shuffle` (13-18), the unseeded RNG that Phase 0b fixes — and the
  `std::shuffle` call whose implementation-defined behaviour Phase 0b also has to replace.
- `include/romanian_whist/Deck.h` — `addCard` / `shuffle` / `operator[]` and nothing else. No
  `size()`, no iteration, and `GameEngine::deck` is private with no accessor, so Phase 0's
  deck-composition and shuffle-determinism tests have nothing to assert against until 0b opens it.
- `src/CardValidator.cpp` — `getWinningCard` (66-77), which copes with a partial trick and so is
  what `getCurrentTrickLeader()` should be built on in Phase 2; it and `getLegalCards` are the
  two signatures Phase 4 has to move to values alongside the strategies.
- `src/Player.cpp` — `addToScore` / `resetCurrentRoundScore` (66-75). `addToScore` writes
  `currentRoundScore`, and `resetCurrentRoundScore` is the actual commit into `totalScore`; this
  is the split behind `getRoundScore()` / `getTotalScore()` and the projected-total note in §3.3.
- `.github/workflows/ci.yml` — the three-platform matrix (ubuntu / windows / macos) that decides
  how portable Phase 0's goldens have to be.
- `src/strategies/RandomCardStrategy.cpp` — `uniform_int_distribution` at lines 21 and 42, the
  other two implementation-defined draws.
- `src/AiMoveProvider.cpp` — the one place that bridges value-choosing strategies to the
  index-returning provider boundary in Phase 4.
- `src/strategies/LowRiskStrategy.cpp` — `getBestBet` (7-22), the only strategy that reads its
  hand to bid and so the only one Phase 5 changes.
- `include/romanian_whist/Player.h` — `getHand()` (30), public and needed by the renderer, which
  is why round-type blinding can never be enforcement (§3.6).
- `../romanian_whist_terminal/src/TerminalRomanianWhist.cpp` — `loop()` (89-174) and
  `playCurrentRoundTricks()` (176-282): the known-correct loop to port into `playRound()`.
- `../romanian_whist_terminal/src/GameView.cpp` — `refreshFromEngine`, which loses its
  `openingSeat` parameter once the engine keeps `getRoundLeaderSeat()`; lines 60 and 80-81 are
  the `getHand()` reads that make hand-blinding pointless, and line 52's `getActual` is one of
  the three call sites Phase 1 has to move.
- `../romanian_whist_terminal/src/ConsoleMoveProvider.cpp` — `layOutHand` (25-63) and `playCard` (107-144). Read them
  before writing Phase 4: the mapping it builds is *display order, legal subset*, not a hand
  index, which is why Phase 4's index boundary adds a translation here rather than removing one.
- `../romanian_whist_terminal/src/TerminalRomanianWhist.cpp` — `markCurrentlyWinning` (284-293),
  the only client code that calls `determineTrickWinner` on a *partial* trick; it is why
  `getCurrentTrickLeader()` exists in §3.3.
- `include/romanian_whist/PlayerList.h` — `std::list<Player>`, whose only reason to be a list is
  the iterator stability Phase 1 stops needing.
- `QT_CLIENT_PLAN.md` — §3-4 are the worked example of the threading contract in §3.8, and the
  reason that contract has to be stated in this document rather than discovered in that one.
