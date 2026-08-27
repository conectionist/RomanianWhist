# Qt desktop client, on the v4 engine

A design sketch for a Qt 6 / C++ GUI client for Romanian Whist, written against the engine as it
exists *after* [ENGINE_V4_PLAN.md](ENGINE_V4_PLAN.md).

> **This document belongs in the Qt client's own repository once that exists.** It lives here for
> now because it depends on the v4 plan and needs to sit beside it. Move it when the repo is
> created, and leave a pointer behind.

---

## 1. Prerequisites

**Do not start this before v4 phase 3 lands.** The client is built around `IGameObserver` and
`GameEngine::run()`, neither of which exists in v3. Starting earlier means writing the trick loop
by hand and then deleting it — the exact duplication the refactor exists to prevent.

| v4 phase | Why this client needs it |
|---|---|
| 0 — tests | Not strictly required, but the client is only as trustworthy as the engine under it |
| 1 — seats | `Seat` is the identity used throughout the UI; iterators would be unusable here |
| 2 — loop + observer | **Load-bearing.** The entire design is `IGameObserver` |
| 3 — `start(GameSetup)` | Setup dialog builds a `GameSetup` directly |
| 4 — cards by value | Strongly desirable: snapshots cross a thread boundary, and copying values is trivial where copying `Card*` is a bug |
| 5 — Forehead/Hidden | Only needed for those round types to render correctly |

Repository layout follows `romanian_whist_terminal`: the engine as a git submodule at
`libs/RomanianWhistEngine`, consumed with `add_subdirectory` and linked as `RomanianWhist::engine`.
Qt arrives via `find_package(Qt6 REQUIRED COMPONENTS Widgets)`.

---

## 2. The core constraint

**The Qt client is architecturally the web backend, not the terminal.**

The terminal can let the engine block, because its "UI" is a blocking `std::cin` — when
`ConsoleMoveProvider` waits for input, there is nothing else the process needs to be doing. Qt has
no such luxury: `engine.run()` on the GUI thread would freeze the event loop, and the window would
stop repainting until the game ended.

So the Qt client needs what the web backend needs:

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
class GameBridge : public QObject, public romanian_whist::IGameObserver
{
    Q_OBJECT
public:
    explicit GameBridge(int humanSeat, PacingOptions pacing);

    // ---- IGameObserver: all called on the GAME thread ----
    void onRoundStarted(const GameEngine& e) override { publish(e); }
    void onBetPlaced(const GameEngine& e, Seat seat, unsigned int bet) override
    {
        publish(e);
        emit betPlaced(seat, bet);
        if(static_cast<int>(seat) != humanSeat) pace(pacing.beat);
    }
    void onBettingComplete(const GameEngine& e) override { publish(e); pace(pacing.beat); }
    void onCardPlayed(const GameEngine& e, Seat seat, const Card& card) override
    {
        publish(e);
        emit cardPlayed(seat, card);
        if(static_cast<int>(seat) != humanSeat) pace(pacing.beat);
    }
    void onTrickWon(const GameEngine& e, Seat seat, unsigned int trickNumber) override
    {
        publish(e);
        emit trickWon(seat, trickNumber);
        pace(pacing.trickDwell);
    }
    void onRoundScored(const GameEngine& e) override { publish(e); emit roundScored(); }
    void onGameOver(const GameEngine& e) override { emit gameOver(e.getStandings()); }

signals:
    void stateChanged(GameSnapshot snapshot);      // BY VALUE - crosses a thread boundary
    void betPlaced(unsigned int seat, unsigned int bet);
    void cardPlayed(unsigned int seat, romanian_whist::Card card);
    void trickWon(unsigned int seat, unsigned int trickNumber);
    void roundScored();
    void gameOver(std::vector<romanian_whist::Standing> standings);

    // Raised by QtMoveProvider, which has no signals of its own.
    void betRequested(BetPrompt prompt);
    void cardRequested(PlayPrompt prompt);

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

    int humanSeat;
    PacingOptions pacing;
};
```

Because `GameBridge` has affinity to the game thread and `MainWindow` to the GUI thread, Qt's
default `AutoConnection` resolves to `QueuedConnection` and marshals for you. That is the Qt
equivalent of the web backend's `queueInLoop`.

`pace()` inside a callback is deliberate, and mirrors the terminal's `Pacer`: blocking the observer
pauses the game, which is exactly what makes bot turns watchable.

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
        emit bridge->betRequested(BetPrompt{ context.handSize,
                                             context.forbiddenBet,
                                             context.roundType,
                                             snapshotHand(context.hand) });
        cv.wait(lock, [this]{ return answer || aborted; });
        if(aborted) throw GameAborted{};
        return static_cast<unsigned int>(*std::exchange(answer, std::nullopt));
    }

    std::optional<std::size_t> playCard(const PlayContext& context) override
    {
        CardValidator validator;
        const std::vector<Card> legal =
            validator.getLegalCards(context.hand, context.trump, context.leadSuit);
        if(legal.empty()) return std::nullopt;

        std::unique_lock lock(mutex);
        emit bridge->cardRequested(buildPlayPrompt(context, legal));
        cv.wait(lock, [this]{ return answer || aborted; });
        if(aborted) throw GameAborted{};
        return *std::exchange(answer, std::nullopt);
    }

    // ---- called on the GUI thread, straight from a click handler ----
    void submit(std::size_t choice)
    {
        { std::lock_guard lock(mutex); answer = choice; }
        cv.notify_one();
    }

    void abort()
    {
        { std::lock_guard lock(mutex); aborted = true; }
        cv.notify_one();
    }

private:
    GameBridge* bridge;                       // non-owning; outlives this
    Seat seat;
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<std::size_t> answer;
    bool aborted = false;
};
```

`submit()` needs no signal or slot. It touches only a mutex, so calling it directly from a click
handler on the GUI thread is safe — and skipping Qt's event queue means the game thread wakes
immediately.

**Emitting a signal from a non-QObject:** `emit bridge->signalName(...)` will not compile, because
signals are protected. Give `GameBridge` small public `raiseBetRequested()` / `raiseCardRequested()`
forwarders, or declare `QtMoveProvider` a friend. The forwarder is cleaner.

### 4.3 `GameController` — owns the thread

```cpp
class GameController : public QObject
{
    Q_OBJECT
public:
    void start(GameSetup setup, int humanSeat);   // builds bridge + provider, spawns the thread
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

The engine itself should be constructed *inside* that slot, so it is born, played and destroyed
entirely on the game thread — the same reasoning as the web backend's `GameSession::threadMain()`.

### 4.4 `MainWindow`

```cpp
connect(bridge, &GameBridge::stateChanged,  this, &MainWindow::render);
connect(bridge, &GameBridge::betRequested,  this, &MainWindow::showBetPrompt);
connect(bridge, &GameBridge::cardRequested, this, &MainWindow::enableHand);
connect(bridge, &GameBridge::trickWon,      this, &MainWindow::flashTrickWinner);
connect(bridge, &GameBridge::gameOver,      this, &MainWindow::showStandings);

void MainWindow::render(const GameSnapshot& s)      // a copy; the engine is untouched
{
    roundLabel->setText(tr("Round %1 of %2").arg(s.round.number).arg(s.round.count));
    trumpWidget->setCard(s.trump);                  // std::optional<Card>
    for(const SeatSnapshot& seat : s.seats)
        seatPanels[seat.seat]->update(seat);
    handWidget->setCards(s.hand);                   // engine order - never re-sort
    trickWidget->setCards(s.table);
}

void MainWindow::onCardClicked(std::size_t index)
{
    handWidget->setEnabled(false);                  // until the next stateChanged arrives
    provider->submit(index);
}
```

> **`handWidget->setCards(s.hand)` — the snapshot's hand order *is* the index contract.**
> Re-sorting inside the widget silently sends the wrong card, exactly as it would in the web
> client. Sort in `orderHandForDisplay()` on the engine side if you want a different order.

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
plain data. Keep it that way; do not add references or `unique_ptr` members to it.

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
    engine->requestStop();     // ends cleanly at the next trick boundary
    provider->abort();         // unblocks a provider parked mid-prompt
    bridge->cancelPacing();    // unblocks an interruptible pace()
    thread.quit();
    thread.wait();             // join BEFORE anything else is destroyed
}
```

`requestStop()` alone is not enough — if the game thread is blocked waiting for a human bet, it
never reaches a boundary at which to check the flag. Equally, `abort()` alone leaves a thread
sleeping in `pace()` for up to `trickDwell`.

`pace()` must therefore be an interruptible wait, not `QThread::sleep` or `std::this_thread::sleep_for`:

```cpp
void GameBridge::pace(std::chrono::milliseconds delay)
{
    std::unique_lock lock(pacingMutex);
    pacingCv.wait_for(lock, delay, [this]{ return pacingCancelled; });
    if(pacingCancelled) throw GameAborted{};
}
```

`GameAborted` propagates out of the provider or observer, through `GameEngine::run()`, and is caught
in the thread's run slot — the same unwind the terminal already uses for a closed stdin, and the
web backend for a dropped socket. Per the v4 plan, an engine that throws out of `run()` is **not
resumable**: destroy it.

---

## 7. UI surface

Minimum to play a full game:

| Widget | Reads from snapshot |
|---|---|
| Setup dialog | *(writes)* builds a `GameSetup` — names, bot levels, structure, the two scoreboard flags |
| Seat panel × N | `seats[i]`: name, bid (only when set), tricks won, cards left, round/total score, streak |
| Trick area | `table` in play order, `isWinning` highlight, trump indicator |
| Hand | `hand` in engine order; illegal cards dimmed and non-clickable |
| Bet prompt | `handSize` range, `forbiddenBet` disabled with a tooltip explaining why |
| Scoreboard | per-round bid/actual/score; `roundScored` is the signal to refresh it |
| Status bar | round number, trick number, round type, whose turn |
| Game over dialog | `getStandings()` |

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

**Phase A — spectator window.** All-bot game, no input. Setup hardcoded. `GameController` +
`GameBridge` + `MainWindow::render`. Proves the thread boundary, the snapshot marshalling and the
metatype registration — the three things most likely to be subtly wrong. *Verify:* a full game
plays out on screen, the window stays responsive throughout, and closing it mid-game exits cleanly.

**Phase B — the human seat.** `QtMoveProvider`, the bet prompt, the clickable hand. This is the
hard part; A exists to make it the only hard part. *Verify:* play a full game; confirm illegal
cards cannot be clicked and the forbidden bid is disabled.

**Phase C — setup dialog and scoreboard.** `GameSetup` from a real dialog, per-round score table,
game-over standings.

**Phase D — polish.** Card animation, pacing controls, round-type presentation (Forehead/Hidden),
keyboard shortcuts, window state persistence via `QSettings`.

---

## 9. A design question this raises for the engine

`GameSnapshot` and `refreshFromEngine()` currently live in the *terminal client*
(`GameView.{h,cpp}`), and the v4 plan has each client build its own.

**With a third client, that stops being obviously right.** All three need the same snapshot of the
same state, and two of the three (Qt, web) need it specifically because they must not read the
engine off-thread. Three hand-maintained copies of the same struct is the same duplication problem
the v4 refactor exists to solve, one level up.

**Recommendation: promote `GameSnapshot` + `refreshFromEngine()` into the engine** as
`romanian_whist::GameSnapshot`, alongside `orderHandForDisplay()`. It is plain data with no UI
dependency, it is exactly what "read the state safely" means for any client, and it gives the Qt
and web clients a supported way to cross a thread boundary instead of each inventing one.

This is a small addition to v4 phase 2 and should be decided before the Qt client starts, not
after.

---

## 10. Comparison: the three clients on one engine

| | Terminal | Qt | Web |
|---|---|---|---|
| Engine runs on | main thread | `QThread` worker | session thread |
| Observer does | draws directly | snapshot → signal | snapshot → JSON |
| Marshalling | none needed | `QueuedConnection` | `queueInLoop` |
| Provider blocks on | `std::cin` | condition variable | condition variable |
| Unblocked by | keypress | click | WebSocket frame |
| Cancellation | EOF → exception | `stop()` → exception | disconnect → exception |
| Rules implemented | none | none | none |

The last row is the point.

---

## 11. Reference files

- [ENGINE_V4_PLAN.md](ENGINE_V4_PLAN.md) — the engine this is written against; §3 is the API surface.
- `../romanian_whist_terminal/src/GameView.{h,cpp}` — `GameSnapshot` and `refreshFromEngine()`,
  reused here (see §9).
- `../romanian_whist_terminal/src/TerminalRomanianWhist.cpp` — the post-v4 observer shape, in its
  simplest form (no thread boundary).
- `../romanian_whist_terminal/src/SetupWizard.h` — `GameConfig`, the shape the setup dialog builds.
- `../romanian_whist_terminal/CMakeLists.txt` — the submodule guard + `add_subdirectory` +
  `RomanianWhist::engine` pattern to copy.
