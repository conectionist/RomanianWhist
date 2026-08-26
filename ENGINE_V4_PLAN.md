# Engine v4: Move the game loop into the engine

A refactor plan for `RomanianWhistEngine`. Written to be handed to an implementer (human or AI)
and executed phase by phase.

---

## 1. Why

The engine models rounds, scoring and card ranking, but **not the play**. There is no
"play a trick" or "play a round" operation. So every client hand-writes the loop that
drives them, and that loop is made of game rules.

`romanian_whist_terminal/src/TerminalRomanianWhist.cpp` is ~218 lines across `loop()` and
`playCurrentRoundTricks()`. Strip the `view.*` / `renderer.*` / `pacer.*` lines and about
175 of them are rules. The UI client decides the lead suit, tracks how many tricks each
player has won, tells the engine who leads next, and calls `calculateScores()` /
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

**Rules that need the loop cannot be enforced.** `RoundType::Forehead` and `RoundType::Hidden`
are stored but ignored — `IMPLEMENTATION_PLAN.md` lists this as an open gap. It has to be a
gap, because the *client* assembles the bidding prompt, so the engine has no opportunity to
withhold the hand.

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

### Two findings from reading the code

**The result-seeding loop in the terminal client is dead code.** `TerminalRomanianWhist.cpp:143-151`
seeds every seat's result to 0 with a comment explaining that a player who wins nothing would
otherwise never be written. But `Round::getActual()` already returns 0 for a missing entry
(`Round.cpp:89-97`), and `bets[name]` value-initialises `Bet` to zeros anyway. The loop changes
nothing except making `hasBet()` true early — which is precisely the hazard it is carefully
sequenced to avoid. It should be deleted, not ported.

**`Deck::shuffle()` cannot be seeded.** It constructs a `std::random_device` and `std::mt19937`
internally on every call (`Deck.cpp:13-18`). There is no way to make a game reproducible, so
**golden-output tests are impossible until this changes**. This is a prerequisite for Phase 0,
not a nice-to-have. `RandomCardStrategy` has the same problem (a fresh `random_device` per call),
so it must be excluded from deterministic tests or made seedable.

---

## 3. Target architecture

The seam moves from *"engine holds state, client drives"* to *"engine drives, client decides and
observes"*.

| Interface | Status | Responsibility |
|---|---|---|
| `IMoveProvider` | unchanged in shape | "What do you want to do?" — bet and card decisions |
| `IGameObserver` | **new** | "Here is what just happened" — rendering, logging, broadcasting |
| `GameEngine::run()` / `playRound()` | **new** | owns the loop |

### 3.1 `Seat` replaces `PlayerList::iterator` in the public API

```cpp
// include/romanian_whist/Seat.h
namespace romanian_whist { using Seat = unsigned int; }
```

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
// pacing and "press Enter to continue" belong - but an observer that blocks on
// network I/O will stall the game for as long as it takes.
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
    virtual void onGameOver(const GameEngine&) {}
};

} // namespace romanian_whist
```

Every callback takes `const GameEngine&` so an observer never needs to capture state or keep a
parallel copy — it reads what it needs at the moment it is told something changed.

`onRoundScored` fires between `calculateScores()` and `commitRoundScores()`, preserving the
terminal's ability to show "what this round was worth" alongside the running total.

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
    ~GameEngine();
    GameEngine(const GameEngine&)            = delete;
    GameEngine& operator=(const GameEngine&) = delete;
    GameEngine(GameEngine&&)                 = delete;   // Round/Trick hold seats now, but
    GameEngine& operator=(GameEngine&&)      = delete;   // moving is still meaningless here

    // ---- setup ----
    // Validates (2..6 seats; names non-empty and unique) and performs every setup
    // step in the one correct order. Throws std::invalid_argument on bad input.
    void start(GameSetup setup);

    // Non-owning. Observers must outlive the engine. Registering the same
    // observer twice is a no-op.
    void addObserver(IGameObserver* observer);
    void removeObserver(IGameObserver* observer);

    // ---- driving ----
    void run();          // while(isInProgress() && !stopRequested) playRound();
    void playRound();    // deal, bet, play every trick, score, advance
    void requestStop();  // ends cleanly at the next trick boundary; status -> Stopped

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

    // ---- bidding rules, for prompting a UI ----
    std::optional<unsigned int> getForbiddenBet() const;
    bool isBetLegal(unsigned int bet) const;

    // ---- scores ----
    std::vector<Standing> getStandings() const;     // sorted desc; carries seat, so
                                                    // duplicate names stay distinguishable
    int getRoundScore(Seat) const;
    int getTotalScore(Seat) const;
};
```

**Removed from the public API** (all become private implementation details of `playRound()`):
`shuffleDeck`, `dealCards`, `placeBet`, `setResult`, `addTrickToCurrentRound`,
`determineTrickWinner`, `setFirstPlayerOfTheRound`, `completeCurrentRound`, `calculateScores`,
`commitRoundScores`, `initializeScoreboard`, `initializeDeck`, `setStatus`,
`getFirstPlayerOfTheRound`, `getNextPlayer`.

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
    Seat leaderSeat;    // moves to each trick's winner
    Seat openerSeat;    // fixed for the round; sets bidding order

public:
    std::optional<unsigned int> getBet(Seat) const;
    unsigned int getTricksWon(Seat) const;   // counts stored tricks whose winner == seat
    const Trick& getCurrentTrick() const;
    std::size_t  getPlayedTrickCount() const;
    // ...
};
```

**`Bet::actual`, `Round::setResult()`, `Round::getActual()` and `Round::hasBet()` are deleted.**
Tricks won are *derived* from the tricks the round already stores, not pushed in by a client.
This removes the `setBet`/`setResult` shared-storage coupling outright, so `getForbiddenBet()`
becomes a straightforward count over `bets` and no longer needs its defensive walk.

`getBet()` returning `std::optional` removes the "0 means either a zero bid or no bid" trap;
`hasBet()` has nothing left to disambiguate.

### 3.5 `Trick`, after

```cpp
class Trick
{
private:
    std::vector<Card> playedCards;   // by value - see Phase 4
    std::optional<Suit> leadSuit;    // empty until the first card is played
    std::optional<Seat> winner;      // empty until the trick is decided
};
```

Storing `Seat` instead of `PlayerList::iterator` also fixes a latent problem: `Trick`'s default
constructor currently leaves `winner` singular, and `addTrickToCurrentRound` copies the trick —
copying a singular iterator is not something to rely on.

### 3.6 `BetContext` gains a hand size, so Forehead/Hidden can be enforced

```cpp
struct BetContext
{
    // EMPTY when the round type forbids the bidder from seeing their own hand
    // (Forehead, Hidden). Use handSize for the legal bid range, never hand.size().
    const std::vector<Card>& hand;
    unsigned int handSize = 0;          // authoritative upper bound on a legal bid

    std::optional<Card> trump;          // empty in 8-card rounds (Phase 4; Card* until then)
    bool isFirstPlayer = false;
    std::optional<unsigned int> forbiddenBet;
    RoundType roundType = RoundType::Normal;
    Seat seat = 0;
};
```

This is what makes the round types real rather than advisory. In a Forehead round the bidder
cannot see their own card but everyone else can; in a Hidden round nobody can. Both mean *the
bidder's* `BetContext.hand` is empty — the difference is only what the renderer shows about
*other* seats, which it decides from `getCurrentRoundType()`.

**Every strategy must be updated to use `context.handSize`, not `context.hand.size()`.**
`RandomCardStrategy::getBestBet` uses `hand.size()` as its upper bound and `LowRiskStrategy`
feeds `hand` to `countLikelyWinners`; left alone, both would bid 0 in every blind round while
`handSize` says 1 is legal — and if 0 is the forbidden bid, they would bid illegally.

`PlayContext` gains `Seat seat` and is otherwise unchanged.

### 3.7 What the terminal client becomes

```cpp
void TerminalRomanianWhist::startGame()
{
    game.addObserver(this);
    game.start(buildSetup(options.demo ? SetupWizard::demoConfig() : wizard.run()));
    game.run();
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

`loop()`, `playCurrentRoundTricks()`, `markCurrentlyWinning()` and `seatOf()` are all deleted.
`openingSeat` and the `tricksWon` vector are deleted. `refreshFromEngine` loses its `openingSeat`
parameter and becomes a pure function of the engine.

`GameView`, `Renderer`, `CardFormat`, `SetupWizard`, `Terminal`, `Input` and `ConsoleMoveProvider`
are untouched.

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
| 0 — testability | **None.** Source-compatible: `Deck` is not reachable from `GameEngine`'s public API, and giving the enums an underlying type does not change `static_cast<int>` comparisons or `switch` statements. | — |
| 1 — seats | Mechanical but broad | `TerminalRomanianWhist.cpp` (every iterator use, `seatOf`), `GameView.cpp` (`hasBet`/`getBet` by name → by seat) |
| 2 — engine owns the loop | **Substantial rewrite** | `TerminalRomanianWhist.{h,cpp}` becomes observer callbacks; `GameView.cpp` loses `openingSeat`; `Pacer` calls move into the callbacks |
| 3 — setup | Small | `applySetup()` → builds a `GameSetup`; `Renderer::drawGameOver` takes `std::vector<Standing>` |
| 4 — cards by value | Moderate | `ConsoleMoveProvider.cpp` (`layOutHand`, index-returning `playCard`), `CardFormat.{h,cpp}`, `GameView.cpp` |
| 5 — Forehead/Hidden | Small but visible | `ConsoleMoveProvider` (blind bid prompt), `Renderer`/`GameView` (whether other seats' cards are shown) |
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

### Phase 0 — Make the engine testable, then pin its behaviour

Nothing else can be done safely first. There are no tests today.

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
- `GameEngine` owns a `std::mt19937`, seeded from `GameSetup::shuffleSeed` when present and
  from `std::random_device` otherwise.
- Add an optional seed to `RandomCardStrategy` (constructor taking `std::uint32_t`), defaulting
  to non-deterministic. Deterministic tests use the seeded form.

**0c. Test target**

- `WHIST_BUILD_TESTS` option, defaulting to `ON` only when `PROJECT_IS_TOP_LEVEL` — so the
  terminal client and the web backend never build the engine's tests.
- Catch2 v3 via `FetchContent`, pinned. Add `enable_testing()` + `ctest`.
- New directory `tests/`, added to `CMakeLists.txt` explicitly (the source list is not globbed).

**0d. Unit tests** — these encode rules and must survive every later phase unchanged:

| Area | Cases |
|---|---|
| `CardValidator::getLegalCards` | leading returns whole hand; must follow lead suit when held; must trump when void in lead and holding trump; free discard when void in both; overtrumping not required; empty hand → empty |
| `CardValidator::beats` | trump over plain; higher trump over lower; lead suit over off-suit; higher rank within suit; two off-suit discards → incumbent stays |
| Forbidden bet | empty for all but the final bidder; empty for the final bidder when bids already exceed the trick count; otherwise exactly `trickCount - total` |
| `isBetLegal` | rejects `> trickCount`; rejects the forbidden value; accepts everything else in range |
| Round scoring | exact bid → `5 + bid`; miss → `-abs(bid - actual)` |
| Streaks | 5 consecutive hits → `+10`; 5 consecutive misses → `-10`; 1-card rounds excluded |
| Scoreboard schedule | 1-8-1 and 8-1-8 shapes; round count `4N + 12` (+2 with `endWithForeheadAndHidden`); opening seat advances one per round; `all1GamesAreForehead` marks every 1-card round |
| Deck composition | 2p → Jack..Ace (16 cards); 4p → Seven..Ace (32); 6p → Three..Ace (48); `Rank::Two` never dealt |
| Setup validation | 1 seat and 7 seats both throw; duplicate and empty names throw; `"John"` / `"john"` are accepted as distinct |

**0e. Golden full-game tests** — the safety net for the whole refactor:

- Fixed `shuffleSeed`, deterministic strategies only (`FirstCard`, `LowRisk`, `Ducking`, plus
  seeded `Random`).
- Run a full game for 2, 4 and 6 players, both structures.
- Assert the **exact final score of every seat**, plus per-round bid/actual for at least one game.
- These tests must drive the game through the *current* API in Phase 0 (duplicating the loop —
  unavoidable, and the last time it ever needs duplicating) and switch to `run()` in Phase 2.

> **Record the golden values by inspection, not by blindly pasting whatever the code emits.**
> Verify by hand that at least one full round's bids, tricks and scores are actually correct
> before freezing them. A golden test that pins a bug is worse than no test.

**Verify:** `ctest --output-on-failure` green; terminal client still plays.

---

### Phase 1 — Seats and the round data model

Mechanical, no behaviour change. The golden tests must produce byte-identical scores.

1. Add `Seat.h`. Convert the public API of `Round`, `Trick`, `Scoreboard` and `GameEngine` from
   `PlayerList::iterator` to `Seat`. Keep iterators inside `PlayerList`.
2. `Round::bets` becomes `std::vector<std::optional<unsigned int>>` indexed by seat.
   `getBet(Seat) -> std::optional<unsigned int>`.
3. **Delete `Bet::actual`, `Round::setResult`, `Round::getActual`, `Round::hasBet`.**
   Add `Round::getTricksWon(Seat)`, counting stored tricks by winner.
4. `Scoreboard::calculateScores` reads `getTricksWon(seat)` instead of `getActual(name)`.
5. `GameEngine::getForbiddenBet()` counts over `bets` directly; delete the defensive
   walk-the-players comment, which no longer describes anything.
6. `Trick` stores `Seat winner`. `Round.h` and `Trick.h` drop their `PlayerList.h` include.
7. Update the terminal client mechanically: `seatOf()` still exists here as a local shim if
   needed, and **the result-seeding loop is deleted** (see §2 — it was already dead code).

**Verify:** golden scores unchanged; terminal plays a full game.

---

### Phase 2 — The engine owns the loop

The core of the refactor.

1. Add `IGameObserver.h`, `GamePhase`, observer registration, and `GameStatus::Stopped`.
2. Add `GameEngine::playRound()` and `run()`. Port the body from
   `TerminalRomanianWhist::loop()` + `playCurrentRoundTricks()` — **this is the reference
   implementation and it is known-correct; port it faithfully rather than rewriting from the
   rules.** Replace each `renderer.drawFrame(view)` with the matching observer callback and
   drop the `pacer.*` calls (pacing moves to the client's observer).
3. `Round` owns the in-flight `Trick`; the engine sets the lead suit and appends cards.
   `getCurrentTrick()` exposes it.
4. Add the live-state accessors: `getPhase`, `getActiveSeat`, `getRoundLeaderSeat`,
   `getTrickLeaderSeat`, `getBiddingOrder`, `getCurrentTrickNumber`, `getTricksWon`.
5. `requestStop()` sets a flag checked at the top of each trick and each round.
6. Make `Player::playCard` / `Player::getBet` private, `friend class GameEngine`.
7. Make the former driving methods private.
8. Switch the golden tests from the duplicated loop to `run()`. **Scores must not change.**
9. Rewrite the terminal client as observer callbacks (§3.7).

> **Guard against double scoring.** `Scoreboard::calculateScores` accumulates into
> `currentRoundScore` via `addToScore`, with no guard — calling it twice doubles the round's
> points. Now that the engine calls it, add an assertion or a phase check so a future edit
> cannot introduce a second call silently.

**Verify:** golden scores unchanged; terminal plays a full game, with pacing and Enter-to-continue
behaving as before.

---

### Phase 3 — Setup consolidation

1. Add `GameSetup`, `SeatSetup`, `GameEngine::start(GameSetup)`.
2. Validate inside `start()`: 2..6 seats (throw `std::invalid_argument` otherwise — this closes
   the `initializeDeck` undefined-behaviour hole for 7+ players), names non-empty and unique.
3. Perform the four setup steps internally, in order. `initializeDeck` no longer takes a player
   count; it reads its own.
4. Delete `addPlayer`, `initializeScoreboard`, `initializeDeck`, `setStatus` from the public API.
5. `getStandings()` returns `std::vector<Standing>` carrying the seat, so duplicate names remain
   distinguishable. Keep `getPlayerScores()` as a deprecated shim or delete it — the terminal's
   `drawGameOver` needs a small update either way.

#### Duplicate names are rejected — decided

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

### Phase 4 — Cards by value

Removes the two subtlest rules in the codebase. Optional in the sense that nothing else depends
on it — but it is the only way to *eliminate* rather than *document* these hazards.

**The problems it removes:**
- `Player::playCard` erases the played card via `std::remove` on `Card*`, so a provider that
  returns a copy silently erases nothing and the hand drifts.
- `Deck::shuffle` permutes values between slots, so any `Card*` cached across a shuffle stays
  valid but silently denotes a different card.

**The change:** make `Player::hand`, `Trick::playedCards`, `PlayContext::hand`,
`PlayContext::playedCards` and `BetContext::hand` hold `Card` by value. After Phase 0a a `Card`
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
2. **The web client's wire protocol is index-based regardless** — a browser can only send back an
   index into the hand it was shown. Matching the engine's boundary to it removes a translation
   step at exactly the place these bugs like to live.
3. **`ConsoleMoveProvider::layOutHand` already builds this exact mapping by hand.** Phase 4 makes
   it native and that function gets shorter.

#### Values inside, index at the boundary

The honest cost of the index is that positions are clumsier than cards *inside* a strategy, which
wants to reason about what a card is, not where it sits. So do not push indices inward:

- `TrickHeuristics` (`mostDangerous`, `leastDangerous`, `chooseDuckingCard`, `chooseWinningCard`)
  and all four `IStrategy` implementations keep working in **values**, returning
  `std::optional<Card>`.
- `CardValidator::getLegalCards` returns `std::vector<Card>` — values, matching the above.
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

**Also update:** `Player`, `Trick`, `Round`, and the `GameEngine` trick loop.

**Verify:** golden scores unchanged — this phase must not alter play at all.

---

### Phase 5 — Enforce Forehead and Hidden

Closes the gap `IMPLEMENTATION_PLAN.md` records.

1. Add `handSize` and `roundType` to `BetContext`.
2. The engine passes an **empty** `hand` when `roundType` is `Forehead` or `Hidden`, with
   `handSize` still set.
3. Update all four strategies to use `context.handSize` for the bid range and to cope with an
   empty hand (§3.6 — otherwise they bid 0 when 0 may be forbidden).
4. `ConsoleMoveProvider` renders the blind prompt; `Renderer` decides from
   `getCurrentRoundType()` whether other seats' single cards are shown.

> **This phase changes play, and the golden scores will move.** That is expected: bots now bid
> blind in rounds where they previously peeked. Re-baseline the golden values — but diff the
> per-round output first and confirm the change is confined to Forehead/Hidden rounds. If a
> Normal round's bids move, something else broke.
>
> An alternative worth considering: keep a `GameSetup` flag to enforce or ignore round types, so
> the old behaviour stays reachable and the golden tests can pin both.

---

### Phase 6 — Documentation and release

1. `README.md`: replace the "the engine does not own the loop — your client does" section with
   the observer model. Document that observers are called synchronously, that blocking pauses
   the game, and that observers must outlive the engine.
2. Document the cancellation contract: `requestStop()` ends cleanly at a trick boundary; an
   exception thrown by a provider propagates out of `run()` and leaves the engine **not
   resumable** — destroy it.
3. Document the one-thread-per-engine rule explicitly. **Do not add internal locking.**
4. `IMPLEMENTATION_PLAN.md`: mark the round-type and test gaps closed.
5. Version to **4.0.0**. Every change here is breaking.
6. Merge `engine-v4` to `master` and move the terminal client's submodule pin onto a master SHA
   (§4). Update its README.

---

## 6. Risks

**Inversion of control.** Clients no longer drive. In practice this is mild — observers are
called synchronously on the same thread, so a breakpoint in `onCardPlayed` behaves exactly like
one in today's loop.

**A blocking observer stalls the game.** Intentional (it is the pacing hook), but it must be
documented, and a future web backend's observer must not block on network I/O.

**The terminal client is the only end-to-end check for pacing and rendering.** The golden tests
cover rules, not presentation. Play a full game by hand at the end of Phases 2 and 5.

**The refactor spans two repositories** and the client cannot build against a partially migrated
engine — see §4. The most likely way to get into trouble here is landing an engine phase on
`master` before the terminal migration that goes with it, which strands the only consumer.

**Phase 4 is a wide, shallow change** touching every strategy and both move providers. It is
also the only phase with a hard guarantee attached: scores must not move at all. If they do,
something in the port is wrong.

**Scope discipline.** Phases 0–3 are the ones that pay for themselves immediately. Phase 4 is
worth doing but can be deferred without blocking a web backend. Phase 5 is a feature, not a
refactor, and could ship separately.

---

## 7. Order of work, condensed

```
0. 2-byte Card -> seedable shuffle -> tests -> golden full-game tests        [no behaviour change]
1. Seat everywhere; bets by seat; results derived; delete setResult          [no behaviour change]
2. IGameObserver; playRound()/run(); engine owns the trick; terminal rewrite [no behaviour change]
3. GameSetup + start() + validation; getStandings()                          [no behaviour change]
4. cards by value; playCard returns an index                                 [no behaviour change]
5. enforce Forehead/Hidden                                                   [BEHAVIOUR CHANGES]
6. docs, version 4.0.0, submodule pin
```

Phases 0–4 must leave the golden scores byte-identical. Only Phase 5 is allowed to move them,
and only in Forehead and Hidden rounds.

---

## 8. Reference files

- `src/GameEngine.cpp` — the primitives being consolidated; `getForbiddenBet()` (lines 96-124)
  explains the `setBet`/`setResult` coupling that Phase 1 removes.
- `src/Round.cpp` — `getBet`/`getActual`/`hasBet` (lines 79-102); the zero-vs-absent ambiguity.
- `src/Scoreboard.cpp` — `calculateScores` (114-156) reads by name and accumulates without a
  guard; `initialize` (13-82) is the round schedule the tests pin.
- `src/Player.cpp` — `playCard` (37-47) is the pointer-identity erase that Phase 4 removes.
- `include/romanian_whist/Card.h` — the two enums that gain `: std::int8_t` in Phase 0a.
- `src/Deck.cpp` — `shuffle` (13-18), the unseeded RNG that Phase 0b fixes.
- `src/AiMoveProvider.cpp` — the one place that bridges value-choosing strategies to the
  index-returning provider boundary in Phase 4.
- `../romanian_whist_terminal/src/TerminalRomanianWhist.cpp` — `loop()` (89-174) and
  `playCurrentRoundTricks()` (176-282): the known-correct loop to port into `playRound()`.
- `../romanian_whist_terminal/src/GameView.cpp` — `refreshFromEngine`, which loses its
  `openingSeat` parameter once the engine keeps `getRoundLeaderSeat()`.
- `../romanian_whist_terminal/src/ConsoleMoveProvider.cpp` — `layOutHand`, the index↔card
  mapping Phase 4 makes native.
