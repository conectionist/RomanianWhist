# Qt desktop client, on the v4 engine

A design sketch for a Qt 6 / C++ GUI client for Romanian Whist, written against the engine as it
exists after [ENGINE_V4_PLAN.md](ENGINE_V4_PLAN.md) — which has now landed in full. The engine is
at 4.1.0 and every prerequisite below is met, so this is buildable today.

> **This document belongs in the Qt client's own repository once that exists.** It lives here for
> now because it depends on the v4 plan and needs to sit beside it. Move it when the repo is
> created, and leave a pointer behind.

---

## 1. Prerequisites

**All met.** This section used to say "do not start before v4 phase 3 lands". Phases 0-6 all
landed, the engine shipped 4.0.0 and is now at **4.1.0**, so nothing here is waiting on the engine
any more. What each phase gave this client, for orientation:

| v4 phase | What this client gets from it |
|---|---|
| 0 — tests | The engine under the GUI is pinned by a Catch2 suite |
| 1 — seats | `Seat` is the identity used throughout the UI. Note it is a **struct with an explicit constructor**, not an integer: `Seat{i}` to build one, `seat.index` to read one |
| 2 — loop + observer | **Load-bearing.** The entire design is `IGameObserver` + `run()` |
| 3 — `start(GameSetup)` | Setup dialog builds a `GameSetup` directly. `shuffleSeed` came with it, which makes a reproducible deal a one-line debugging aid |
| 4 — cards by value | `CardValidator::getLegalCards(hand, trump, leadSuit)` and `PlayContext::hand` are values, so a snapshot copy is trivial where copying `Card*` was a bug |
| 5 — Forehead/Hidden | `canSeeHand()` and `RoundType`, which the table and the bid prompt both need |
| 4.1.0 (post-plan) | `Standing::place` and `getWinners()` — the engine ranks the standings, so no client decides a tie for itself |

Repository layout follows `romanian_whist_terminal`: the engine as a git submodule at
`libs/RomanianWhistEngine`, consumed with `add_subdirectory` and linked as `RomanianWhist::engine`.
Qt arrives via `find_package(Qt6 REQUIRED COMPONENTS Widgets)`.

---

## 2. The core constraint

**The Qt client is architecturally the *planned* web backend, not the terminal.**

The web backend named throughout this document **does not exist yet** — see
[ENGINE_V4_PLAN.md](ENGINE_V4_PLAN.md) §1. Where the text below compares against it, read that as
"the shape the two share", not as code to go and copy. The Qt client will be the first client to
run the engine off the UI thread, and so the first to find out where this is wrong.

The terminal can let the engine block, because its "UI" is a blocking `std::cin` — when
`ConsoleMoveProvider` waits for input, there is nothing else the process needs to be doing. Qt has
no such luxury: `engine.run()` on the GUI thread would freeze the event loop, and the window would
stop repainting until the game ended.

So the Qt client needs what a web backend would need:

1. The engine on a **worker thread**.
2. An **observer** that marshals events out to the GUI thread.
3. A **move provider** that blocks the game thread until the GUI answers.

### The two directions

```
GUI thread                            game thread (QThread)
──────────                            ─────────────────────
MainWindow                            engine.run()
   ▲                                     │
   │  Qt signals (queued),                ├─ IGameObserver ──► snapshot ──┐
   │  GameSnapshot BY VALUE      ◄────────┴──────────────────────────────┘
   │
   └─ click ──► provider->submit(i) ──► condition_variable ──► unblocks playCard()
```

Only the outbound direction needs Qt's machinery. Inbound is a plain direct call, because the
provider is thread-safe by construction — it touches nothing but a mutex and a condition variable.

---

## 3. The cardinal rule

`IGameObserver` hands you `const GameEngine&`. In the terminal you read it directly, because the
callback runs on the same thread as the rendering.

**In Qt you must not do that.** The engine has no locking anywhere, so a GUI-thread read racing the
game thread is a data race, and the failure mode is a torn read of a hand mid-mutation — a card
that is half-removed, or a `std::vector` resized under a loop.

> **Snapshot on the game thread. Emit the snapshot by value. The GUI thread touches nothing but
> its own copy.**

Never emit a signal carrying `const GameEngine&`, `const Round&`, `const Player&`, or a pointer to
anything the engine owns. Signal payloads must be self-contained values.

---

## 4. Components

### 4.1 `GameBridge` — the observer

Lives on the game thread. Every method runs there.

```cpp
// This client's own pacing knobs. The terminal's Pacer (src/Pacer.h) is a class
// with beat()/trickPause()/roundPause() and a skip flag, not an options struct -
// the delays are the part worth copying, not the type.
struct Pacing
{
    std::chrono::milliseconds beat{450};
    std::chrono::milliseconds trickDwell{1200};
};

class GameBridge : public QObject, public romanian_whist::IGameObserver
{
    Q_OBJECT
public:
    explicit GameBridge(Seat humanSeat, Pacing pacing);

    // ---- IGameObserver: all called on the GAME thread ----
    // The engine declares thirteen callbacks; these are the ones this client
    // acts on. The rest stay defaulted no-ops.

    // Fires while the engine is still idle - set up but not yet dealt. The one
    // callback from which run() may legally be called (see section 5).
    void onGameStarted(const GameEngine& e) override { publish(e); }

    void onRoundStarted(const GameEngine& e) override { publish(e); }

    // Whose turn it is to bid. Drives the "waiting on Ana" indicator; the human
    // seat's own prompt comes from QtMoveProvider, not from here.
    void onBetRequested(const GameEngine& e, Seat seat) override
    {
        publish(e);
        emit turnChanged(seat.index);
    }

    void onBetPlaced(const GameEngine& e, Seat seat, unsigned int bet) override
    {
        publish(e);
        emit betPlaced(seat.index, bet);
        if(seat != humanSeat) pace(pacing.beat);
    }
    void onBettingComplete(const GameEngine& e) override { publish(e); pace(pacing.beat); }

    void onTrickStarted(const GameEngine& e, unsigned int trickNumber, Seat leader) override
    {
        publish(e);
        emit trickStarted(trickNumber, leader.index);
    }

    void onCardRequested(const GameEngine& e, Seat seat) override
    {
        publish(e);
        emit turnChanged(seat.index);
    }

    void onCardPlayed(const GameEngine& e, Seat seat, const Card& card) override
    {
        publish(e);
        emit cardPlayed(seat.index, card);
        if(seat != humanSeat) pace(pacing.beat);
    }
    void onTrickWon(const GameEngine& e, Seat seat, unsigned int trickNumber) override
    {
        publish(e);
        emit trickWon(seat.index, trickNumber);
        pace(pacing.trickDwell);
    }
    void onRoundScored(const GameEngine& e) override { publish(e); emit roundScored(); }
    void onRoundComplete(const GameEngine& e) override { publish(e); pace(pacing.trickDwell); }

    // Publish first: the final state is what the game-over dialog is drawn over.
    // getWinners() rather than standings.front() - a drawn game has more than
    // one winner, and the engine is what decides that (4.1.0).
    void onGameOver(const GameEngine& e) override
    {
        publish(e);
        emit gameOver(e.getStandings(), e.getWinners());
    }

    // requestStop() was honoured. This, NOT onGameOver, is what a window closed
    // mid-game produces - see section 6. The round it landed in is left
    // unscored, so there is nothing to announce; the window is closing anyway.
    void onGameStopped(const GameEngine&) override { emit gameStopped(); }

signals:
    void stateChanged(GameSnapshot snapshot);      // BY VALUE - crosses a thread boundary
    void turnChanged(unsigned int seat);
    void betPlaced(unsigned int seat, unsigned int bet);
    void trickStarted(unsigned int trickNumber, unsigned int leader);
    void cardPlayed(unsigned int seat, romanian_whist::Card card);
    void trickWon(unsigned int seat, unsigned int trickNumber);
    void roundScored();
    void gameOver(std::vector<romanian_whist::Standing> standings,
                  std::vector<romanian_whist::Standing> winners);
    void gameStopped();

    // Raised by QtMoveProvider through the forwarders below, since it has no
    // signals of its own.
    void betRequested(BetPrompt prompt);
    void cardRequested(PlayPrompt prompt);

public:
    // QtMoveProvider is not a QObject and signals are protected, so
    // `emit provider->bridge->betRequested(...)` will not compile. These are
    // the seam. Both are called on the GAME thread, from inside the provider.
    void raiseBetRequested(BetPrompt prompt) { emit betRequested(std::move(prompt)); }
    void raiseCardRequested(PlayPrompt prompt) { emit cardRequested(std::move(prompt)); }

private:
    void publish(const GameEngine& engine)
    {
        // Snapshotted HERE, on the game thread. See section 3: the GUI thread
        // must never reach into the engine, which has no thread safety at all.
        GameSnapshot snapshot;
        refreshFromEngine(snapshot, engine);
        emit stateChanged(snapshot);               // copied into the queued event
    }

    void pace(std::chrono::milliseconds delay);    // interruptible - see section 6

    Seat humanSeat;
    Pacing pacing;
};
```

**`Seat` is a struct, not an integer.** `Seat{i}` builds one, `seat.index` reads one, and `==` /
`!=` compare them. It has **no default constructor**, so it can never be a queued signal payload —
which is why every signal above carries `unsigned int seat` and every callback body spells
`seat.index`. Comparing seats (`seat != humanSeat`) is fine and clearer than comparing indices.

Because `GameBridge` has affinity to the game thread and `MainWindow` to the GUI thread, Qt's
default `AutoConnection` resolves to `QueuedConnection` and marshals for you — the same job a web
backend's event-loop post would do.

`pace()` inside a callback is deliberate, and mirrors the terminal's `Pacer`: blocking the observer
pauses the game, which is exactly what makes bot turns watchable. One note carried over from the
v4 plan: the terminal found that `Pacer::resetSkip()` belongs at the end of `onBettingComplete`,
not in `onRoundStarted`. If this client grows a skip key, it inherits that.

### 4.2 `QtMoveProvider` — the blocking seam

```cpp
// Deliberately NOT a QObject. The engine owns this via unique_ptr and destroys
// it on the game thread; destroying a QObject with GUI-thread affinity from
// another thread is a problem you do not want to debug.
class QtMoveProvider : public romanian_whist::IMoveProvider
{
public:
    QtMoveProvider(GameBridge* _bridge, Seat _seat);

    // ---- called on the GAME thread. Blocks until the GUI answers. ----
    unsigned int makeBet(const BetContext& context) override
    {
        std::unique_lock lock(mutex);

        // BetContext has no handSize: the hand's size IS the round's trick
        // count, and so the upper bound on a legal bid.
        //
        // roundType matters here beyond presentation. In Forehead and Hidden
        // the bidder is not entitled to their own hand's contents - the engine
        // still passes the real hand, having no other to give - so the prompt
        // must not show it back to them.
        bridge->raiseBetRequested(BetPrompt{ context.hand.size(),
                                            context.forbiddenBet,
                                            context.roundType,
                                            context.isFirstPlayer,
                                            snapshotHand(context.hand) });
        cv.wait(lock, [this]{ return answer || abandoned; });
        if(abandoned) throw GameAbandoned{};
        return static_cast<unsigned int>(*std::exchange(answer, std::nullopt));
    }

    std::optional<std::size_t> playCard(const PlayContext& context) override
    {
        CardValidator validator;
        const std::vector<Card> legal =
            validator.getLegalCards(context.hand, context.trump, context.leadSuit);
        if(legal.empty()) return std::nullopt;

        std::unique_lock lock(mutex);
        bridge->raiseCardRequested(buildPlayPrompt(context, legal));
        cv.wait(lock, [this]{ return answer || abandoned; });
        if(abandoned) throw GameAbandoned{};

        // std::exchange clears it. Leaving it set means the next turn returns
        // this same index without ever waiting, and the engine throws on the
        // illegal play - see README.md, "Abandoning a game whose provider is
        // parked".
        return *std::exchange(answer, std::nullopt);
    }

    // ---- called on the GUI thread, straight from a click handler ----
    void submit(std::size_t choice)
    {
        { std::lock_guard lock(mutex); answer = choice; }
        cv.notify_one();
    }

    void abandon()
    {
        { std::lock_guard lock(mutex); abandoned = true; }
        cv.notify_one();
    }

private:
    GameBridge* bridge;                       // non-owning; outlives this
    Seat seat;
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<std::size_t> answer;
    bool abandoned = false;
};
```

`submit()` needs no signal or slot. It touches only a mutex, so calling it directly from a click
handler on the GUI thread is safe — and skipping Qt's event queue means the game thread wakes
immediately.

**Emitting a signal from a non-QObject:** `emit bridge->signalName(...)` will not compile, because
signals are protected. Hence the public `raiseBetRequested()` / `raiseCardRequested()` forwarders on
`GameBridge` (§4.1), which is why the calls above are plain method calls with no `emit`. Declaring
`QtMoveProvider` a friend would also work; the forwarder is cleaner.

**`GameAbandoned` is the README's name for this**, and it is worth matching:
[README.md](README.md) "Abandoning a game whose provider is parked" documents wake-and-throw as the
supported way out of a parked provider, with this exact shape. `std::nullopt` is not an
alternative — the engine reads it as "no legal play", which it treats as a bug.

### 4.3 `GameController` — owns the thread

```cpp
class GameController : public QObject
{
    Q_OBJECT
public:
    // GameSetup is MOVE-ONLY - it owns the seats' unique_ptr<IMoveProvider> -
    // so every call site reads start(std::move(setup), seat). start() also
    // moves from the setup before validating it, so a rejected setup cannot be
    // retried: build a fresh one.
    void start(GameSetup setup, Seat humanSeat);  // builds bridge + provider, spawns the thread
    void stop();                                  // see section 6

    GameBridge* bridge() const;                   // for MainWindow to connect to
    QtMoveProvider* provider() const;             // non-owning; valid until stop() returns

private:
    QThread thread;
    std::unique_ptr<GameEngine> engine;           // constructed ON the game thread
    GameBridge* gameBridge = nullptr;
    QtMoveProvider* movePlayer = nullptr;         // owned by the engine
};
```

Use the **worker-object pattern**, not a `QThread` subclass: create the objects, `moveToThread()`
them, connect `QThread::started` to a slot that runs the game.

The engine itself must be constructed *inside* that slot, so it is born, played and destroyed
entirely on the game thread. This is not a preference: [README.md](README.md) "Threading, stopping,
and failure" opens with **one thread per engine, and no internal locking**, and `requestStop()` is
the only member of the whole class that is safe to touch from elsewhere.

### 4.4 `MainWindow`

```cpp
connect(bridge, &GameBridge::stateChanged,  this, &MainWindow::render);
connect(bridge, &GameBridge::betRequested,  this, &MainWindow::showBetPrompt);
connect(bridge, &GameBridge::cardRequested, this, &MainWindow::enableHand);
connect(bridge, &GameBridge::trickWon,      this, &MainWindow::flashTrickWinner);
connect(bridge, &GameBridge::gameOver,      this, &MainWindow::showStandings);
connect(bridge, &GameBridge::gameStopped,   this, &MainWindow::close);

// Field names follow the terminal's GameView (src/GameView.h), which is what
// this type is modelled on - see section 9.
void MainWindow::render(const GameSnapshot& s)      // a copy; the engine is untouched
{
    roundLabel->setText(tr("Round %1 of %2").arg(s.roundNumber).arg(s.roundCount));
    trumpWidget->setCard(s.hasTrump, s.trump);      // no trump in an 8-card round
    for(const SeatView& seat : s.seats)
        seatPanels[seat.seatIndex]->update(seat);
    handWidget->setEntries(s.hand);                 // HandEntry, not Card - see below
    trickWidget->setCards(s.table);                 // PlayedCardView: seat, card, isWinning
}

// handIndex runs parallel to the displayed hand: handIndex[i] is the position
// in the ENGINE's hand of the card drawn at display slot i. Built alongside the
// snapshot, thrown away and rebuilt on the next one.
void MainWindow::onCardClicked(std::size_t displayPosition)
{
    handWidget->setEnabled(false);                  // until the next stateChanged arrives
    provider->submit(handIndex[displayPosition]);   // back to an engine hand index
}
```

> **Three orderings meet in the hand widget, and no two of them coincide.** The terminal hit this
> first and solved it in `HandLayout` (`src/HandLayout.h`), whose header comment enumerates them:
>
> 1. the hand as the **engine** holds it — deal order, and what `playCard()`'s index must refer to;
> 2. **display** order — trump suit first, then a fixed alternating-colour order, high rank first;
> 3. the **clickable legal subset**, which is what the player actually picks from.
>
> `layOutHand()` returns cards in display order plus `choices`, where `choices[n - 1]` is the hand
> position for the number the player types. **A GUI needs the mapping one step earlier:** a click
> lands on a display slot, not on a typed number, so what this client wants is (2) → (1) over the
> whole hand — the `handIndex` vector above — with (3) reduced to a `legal` flag that decides
> whether the slot is clickable at all. `HandEntry` already carries that flag.
>
> Same idea, one indirection fewer. Carrying positions rather than cards is the whole trick:
> sorting for display is fine and desirable, and it is only unsafe if the way back is not carried
> with it. Getting it wrong means the player clicks one card and plays another, which no rule in
> the engine would object to.
>
> An earlier draft of this document said "never re-sort" and pointed at an
> `orderHandForDisplay()` on the engine side. **No such function exists**, and the prohibition was
> the wrong fix for the right worry.

---

## 5. Qt specifics worth getting right

**Register the metatypes.** Queued connections need custom payload types registered:

```cpp
Q_DECLARE_METATYPE(GameSnapshot)
Q_DECLARE_METATYPE(romanian_whist::Card)
Q_DECLARE_METATYPE(BetPrompt)
Q_DECLARE_METATYPE(PlayPrompt)
// in main(), before any connect():
qRegisterMetaType<GameSnapshot>("GameSnapshot");
```

Qt 6 auto-registers in many cases, but explicit registration turns a silent runtime "cannot queue
argument of type X" warning into a compile-time guarantee.

**Payload types must be copyable and default-constructible.** `GameSnapshot` already is — it is
plain data. Keep it that way; do not add references or `unique_ptr` members to it. `Card` qualifies
too (it has a default constructor). **`Seat` and `Standing` do not**: `Seat` is declared
`explicit constexpr Seat(unsigned int)` with no default, and `Standing` holds one. So a bare `Seat`
can never be a queued argument — carry `seat.index` instead. A `std::vector<Standing>` is fine,
because the *vector* is default-constructible.

**The GUI thread calls no engine method — none.** Not a getter, not `getStandings()`. Everything it
needs arrives in a snapshot. `requestStop()` is the sole exception in the entire class, and even
`addObserver()`/`removeObserver()` are explicitly not thread-safe.

**The engine also guards against re-entrancy, and a GUI can trip it.** Both throw
`std::logic_error` rather than corrupt state:

- `addObserver()` / `removeObserver()` from inside any callback — the engine is mid-iteration over
  its observer list. An observer that wants to detach sets a flag and is detached between rounds.
- `run()` / `playRound()` from a move provider, or from any callback except `onGameStarted()`. A
  callback that wants the game to end calls `requestStop()`.

**Do not subclass `QThread`.** Worker-object pattern.

**Signals from the game thread are safe; direct method calls on GUI objects are not.** Never call
`widget->setText()` from the game thread. If you find yourself wanting to, you need another signal.

---

## 6. Shutdown and cancellation

This is the part that bites, and it is worth writing first rather than last.

Closing the window while the game thread is parked inside `cv.wait` needs both halves:

```cpp
void GameController::stop()
{
    engine->requestStop();     // ends cleanly at the next round, bid or trick boundary
    provider->abandon();       // unblocks a provider parked mid-prompt
    bridge->cancelPacing();    // unblocks an interruptible pace()
    thread.quit();
    thread.wait();             // join BEFORE anything else is destroyed
}
```

`requestStop()` alone is not enough — if the game thread is blocked waiting for a human bet, it
never reaches a boundary at which to check the flag. The engine says so itself: *"It cannot
interrupt a move provider already parked waiting for a human... Unparking that provider is the
client's job, and throwing from it is the supported way out."* Equally, `abandon()` alone leaves a
thread sleeping in `pace()` for up to `trickDwell`.

**Which of the two lands decides what the observer sees**, and this is worth getting straight:

| How it ended | What fires | State |
|---|---|---|
| `requestStop()` caught at a boundary | `onGameStopped()` — **not** `onGameOver()` | status `Stopped`, round left unscored, engine not resumable |
| `GameAbandoned` thrown from a parked provider | nothing; the exception unwinds `run()` | engine mid-round, not resumable |
| The game simply finished | `onGameOver()` | status `Finished` |

So a window closed mid-game usually produces `onGameStopped()` or an exception, and *never*
`onGameOver()` — a client that only handles `onGameOver()` is silent on the exact path it was
written for. Note also that a stop requested once the final round has been scored is a **no-op**:
the game stays `Finished` and `onGameOver()` is what fired.

`pace()` must be an interruptible wait, not `QThread::sleep` or `std::this_thread::sleep_for`:

```cpp
void GameBridge::pace(std::chrono::milliseconds delay)
{
    std::unique_lock lock(pacingMutex);
    pacingCv.wait_for(lock, delay, [this]{ return pacingCancelled; });
    if(pacingCancelled) throw GameAbandoned{};
}
```

`GameAbandoned` propagates out of the provider or observer, through `GameEngine::run()`, and is
caught in the thread's run slot. Per the engine's own contract, an engine that throws out of `run()`
is **not resumable**: catch it outside `run()`, destroy the engine, and restore whatever you set up
before the call, because nothing else will.

---

## 7. UI surface

Minimum to play a full game:

| Widget | Reads from snapshot |
|---|---|
| Setup dialog | *(writes)* builds a `GameSetup` — names, bot levels, `structure`, `endWithForeheadAndHidden`, `all1GamesAreForehead`, and optionally `shuffleSeed`. `SetupWizard.h`'s `GameConfig` is the shape to copy |
| Seat panel × N | `seats[i]`: `name`, `bet` (only when `hasBet`), `tricksWon`, `cardsLeft`, `roundScore`/`totalScore`, `winStreak`/`lossStreak`, `bidOrder` |
| Trick area | `table` (`PlayedCardView`: `seatIndex`, `card`, `isWinning`) in play order; trump from `hasTrump`/`trump` |
| Hand | `hand` (`HandEntry`: `card`, `legal`, `choice`) in display order; illegal cards dimmed and non-clickable. Other seats' hands come from `seats[i].hand`, non-empty only where `canSeeHand()` allows — a Forehead round |
| Bet prompt | `0..context.hand.size()`; `forbiddenBet` disabled with a tooltip explaining why. **Blind in Forehead and Hidden** — do not show the bidder their own hand back |
| Scoreboard | per-round bid/actual/score; `roundScored` is the signal to refresh it |
| Status bar | round number, trick number, `roundType`, whose turn (`turnChanged`) |
| Game over dialog | `getStandings()` for the table **and `getWinners()` for the headline**. A drawn game has more than one winner and `standings.front()` names only one of them; the terminal renders it as a draw |

**Widgets vs QML:** `QGraphicsScene` / `QGraphicsView` is the natural fit for a card table — card
items are movable, animatable, and hit-testable for free, and `QGraphicsItem` animation covers
dealing and trick collection without hand-rolled painting. Plain `QWidget` with a custom
`paintEvent` is simpler but you will write the layout maths yourself. QML is the nicest for
animation but adds a second language and a QML/C++ bridge on top of the thread bridge you already
have. **Recommendation: `QGraphicsView` for the table, ordinary widgets for panels and dialogs.**

**Card art:** SVG via `QSvgRenderer` scales cleanly and avoids shipping raster sets per DPI. Public
domain deck SVGs exist; otherwise rank/suit glyphs drawn onto a rounded rect look fine and cost
nothing.

---

## 8. Phases

Each ends in something runnable.

**Phase A — spectator window.** All-bot game, no input. Setup hardcoded, with a fixed
`shuffleSeed` so a misrender is reproducible. `GameController` + `GameBridge` +
`MainWindow::render`. Proves the thread boundary, the snapshot marshalling and the metatype
registration — the three things most likely to be subtly wrong. *Verify:* a full game plays out on
screen, the window stays responsive throughout, and closing it mid-game exits cleanly through
`onGameStopped()` with no leaked thread. Note the parked-provider half of §6 cannot be exercised
here — an all-bot game never parks — which is why B repeats the shutdown test.

**Phase B — the human seat.** `QtMoveProvider`, the bet prompt, the clickable hand. This is the
hard part; A exists to make it the only hard part. *Verify:* play a full game; confirm illegal
cards cannot be clicked, the forbidden bid is disabled, the card clicked is the card played (the
`HandLayout` translation of §4.4), and closing the window **while the bet prompt is open** exits
cleanly — that is the `GameAbandoned` path, and it is the one §6 exists for.

**Phase C — setup dialog and scoreboard.** `GameSetup` from a real dialog, per-round score table,
game-over standings and winners. *Verify:* a game deliberately ended in a draw announces both
seats.

**Phase D — polish.** Card animation, pacing controls, round-type presentation (Forehead/Hidden),
keyboard shortcuts, window state persistence via `QSettings`.

---

## 9. A design question this raises for the engine

**Still open.** Nothing was promoted into the engine during v4, so the snapshot type still lives
only in the terminal — as `struct GameView` with `refreshFromEngine(GameView&, const GameEngine&)`
in `GameView.{h,cpp}`. (Earlier drafts of this document called it `GameSnapshot`; that is this
client's name for its own copy, not the terminal's.)

**With a second client, sharing it stops being obviously optional.** Both need the same snapshot of
the same state, and this one needs it specifically because it must not read the engine off-thread.

But `GameView` has since accumulated fields the engine has no business owning, and that changes the
answer from the one an earlier draft gave:

| Engine-derivable — could move | Client-only — could not |
|---|---|
| round number/count, trick number/count, `roundType` | `message`, `hint`, `error` |
| `hasTrump`/`trump`, `leadSuit` | `elapsedMinutes` (the engine has no wall clock) |
| per-seat name, bet, tricks won, cards left, scores, streaks, bid order | `HandEntry::choice` (a typed keyboard number) |
| `table` from `getCurrentTrick()`, with the `isWinning` highlight | `Phase`, a UI phase distinct from `GamePhase` |
| per-seat visible hands via `canSeeHand()` | `humanSeat`, `activeSeat`, display ordering |

`refreshFromEngine()`'s own header comment already draws that line: *"A pure function of the engine.
What is left to the caller is only what the engine does not model."*

**Revised recommendation: promote the left column only**, as `romanian_whist::GameSnapshot` plus a
`snapshotFrom(const GameEngine&)`, and let each client wrap it in whatever view type it needs. The
left column is exactly what "read the state safely off-thread" means; the right column is UI and
belongs in the UI. `layOutHand()` stays client-side with it — the display ordering it produces is a
presentation choice, not an engine one.

Worth deciding before this client starts, since it determines whether `GameSnapshot` here is a new
struct or a thin wrapper. It is not a blocker either way: building it client-side first and
promoting the common part afterwards is a legitimate order.

---

## 10. Comparison: the three clients on one engine

Terminal exists; Qt is this document; **web is still hypothetical** and its column is what the
shape would be, not a description of code.

| | Terminal *(built)* | Qt *(this document)* | Web *(hypothetical)* |
|---|---|---|---|
| Engine runs on | main thread | `QThread` worker | session thread |
| Observer does | draws directly | snapshot → signal | snapshot → JSON |
| Marshalling | none needed | `QueuedConnection` | post to the event loop |
| Provider blocks on | `std::cin` | condition variable | condition variable |
| Unblocked by | keypress | click | WebSocket frame |
| Cancellation | EOF → exception | `requestStop()` + `GameAbandoned` | disconnect → exception |
| Rules implemented | none | none | none |

The last row is the point.

---

## 11. Reference files

**Engine (this repository), at 4.1.0:**

- [README.md](README.md) — the API surface this is written against. "Threading, stopping, and
  failure" is the contract §3 and §6 work through, and it points back here for the worked example;
  "Abandoning a game whose provider is parked" is §4.2's exception, under its canonical name;
  "Watching a game" is the callback order.
- [CHANGELOG.md](CHANGELOG.md) — 4.1.0 added `Standing::place` and `getWinners()`, which §7's
  game-over row depends on.
- `include/romanian_whist/IGameObserver.h` — all thirteen callbacks, and exactly what is readable
  from each. §4.1 implements twelve of them and defaults the rest.
- `include/romanian_whist/Seat.h` — why a seat is a struct, and why that is deliberate.
- `include/romanian_whist/GameEngine.h` — `GameSetup`, `Standing`, and the re-entrancy rules on
  `addObserver()`/`run()` noted in §5.
- [ENGINE_V4_PLAN.md](ENGINE_V4_PLAN.md) — why the engine has that shape; §3 is the design
  rationale behind the README's surface.

**Terminal client (`../romanian_whist_terminal`), at 2.2.0:**

- `src/GameView.{h,cpp}` — `struct GameView` and `refreshFromEngine()`: the snapshot this client's
  `GameSnapshot` is modelled on (see §9).
- `src/HandLayout.{h,cpp}` — `layOutHand()` and `HandLayout::choices`, the display-order↔engine-index
  translation §4.4 depends on. Its header comment is the best statement of the problem anywhere in
  either repository.
- `src/Pacer.{h,cpp}` — the pacing behaviour §4.1's `Pacing` struct borrows its delays from,
  including the skip mechanism this client does not have yet.
- `src/TerminalRomanianWhist.cpp` — the post-v4 observer shape in its simplest form, with no thread
  boundary. Note it draws the game-over screen *after* `run()` returns rather than from
  `onGameOver()`, which a GUI cannot do.
- `src/ConsoleMoveProvider.cpp` — the other `IMoveProvider`, including the blind bid prompt that
  §7's Forehead/Hidden row calls for.
- `src/SetupWizard.h` — `GameConfig`, the shape the setup dialog builds.
- `CMakeLists.txt` — the submodule guard + `add_subdirectory` + `RomanianWhist::engine` pattern to
  copy, still current.
