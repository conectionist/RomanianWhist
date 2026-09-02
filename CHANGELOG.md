# Changelog

This project follows [Semantic Versioning](https://semver.org/). The version lives in
`CMakeLists.txt` and reaches consumers as `romanian_whist::VersionString`.

## 4.0.0

**The engine owns the game loop.** In 3.x a client wrote the loop itself out of engine
primitives — deal, ask each player to bid, resolve each trick, score, advance — which meant every
client hand-wrote the same game rules, no client's loop could be tested, and rules that needed the
loop (the Forehead and Hidden rounds) had nowhere to live. The loop is now
`GameEngine::run()`/`playRound()`, and a client decides moves (`IMoveProvider`) and renders events
(`IGameObserver`).

This is a breaking release throughout. See [README.md](README.md) for the shape of a v4 client.

### Migration

| 3.0.0 | 4.0.0 |
|---|---|
| `addPlayer()` + `initializeScoreboard()` + `initializeDeck()` | `start(GameSetup)` — one validated call |
| a hand-written `while(isInProgress())` loop | `run()` / `playRound()`, plus `IGameObserver` to render |
| `setStatus()`, `shuffleDeck()`, `dealCards()`, `placeBet()`, `calculateScores()`, `commitRoundScores()`, `completeCurrentRound()`, `determineTrickWinner()` | private — `playRound()` is their only caller |
| `PlayerList::iterator` in the public API | `Seat` |
| `getFirstPlayerOfTheRound()` / `setFirstPlayerOfTheRound()` / `getNextPlayer()` | `getRoundLeaderSeat()` / `getTrickLeaderSeat()` / `getNextSeat()` |
| `IMoveProvider::playCard()` → `Card*` | → `std::optional<std::size_t>`, an index into `context.hand` |
| `IStrategy::getBestChoice()` → `Card*` | → `std::optional<Card>` |
| `Card*` in `BetContext`, `PlayContext`, `Trick`, `CardValidator` | cards by value; `nullptr` becomes an empty `std::optional` |
| `getCurrentTrumpCard()` → `Card*` | → `std::optional<Card>` |
| `getPlayerScores()` / `getPlayerRoundScores()` | `getStandings()` / `getRoundScore(Seat)` / `getTotalScore(Seat)` |
| `Round::hasBet(name)` / `Round::getBet(name)` | `GameEngine::getBet(Seat)` → `std::optional<unsigned int>` |
| `Round::getActual(name)` / `GameEngine::setResult()` | `getTricksWon(Seat)` — derived from the round's stored tricks; there is no setter |
| `Round::getOpeningPlayer()` / `Round::getFirstPlayer()` | `getRoundLeaderSeat()` / `getTrickLeaderSeat()` |
| reaching a `Player` to call `playCard`/`getBet` | both are private, with `friend class GameEngine` |

### Added

- `IGameObserver` — thirteen no-op callbacks covering the whole game, called synchronously on the
  thread running it. `addObserver()` / `removeObserver()`.
- `GameEngine::run()` and `playRound()`, and `requestStop()` to end a game cleanly at the next
  round, bid or trick boundary. A stop never scores the round it lands in, and yields
  `onGameStopped()` rather than `onGameOver()`.
- `GameSetup` / `SeatSetup` and `start()`, which validates the whole setup — 2–6 seats, non-empty
  names, no duplicates — before applying any of it, and leaves the engine untouched if it refuses.
- `Seat`, and per-card seats in `Trick` via `PlayedCard`, so no client has to reconstruct turn
  order arithmetically.
- `canSeeHand(viewer, holder)` — the Forehead/Hidden visibility rule, in one place. It is a rule,
  not a guard; see the README before building a networked client on it.
- `getPhase()`, `isSetUp()`, `getActiveSeat()`, `getCurrentTrick()`, `getCurrentTrickLeader()`,
  `getCurrentTrickNumber()`, `getBiddingOrder(Seat)`, `getRound(index)`, `getStandings()`.
- `GameSetup::shuffleSeed` and `RandomCardStrategy(std::uint32_t)` for reproducible games, on a
  hand-rolled Fisher-Yates shuffle that gives the same sequence on every standard library.
- A Catch2 test suite, gated on `WHIST_BUILD_TESTS` (default: `PROJECT_IS_TOP_LEVEL`), run on
  Linux, Windows and macOS. The engine had no automated tests before this release.
- [docs/RULES.md](docs/RULES.md) — the rules the engine implements, alongside the code for each.

### Changed — behaviour

- **Forehead and Hidden rounds are played.** They were stored in the schedule and ignored. A
  bidder is now told which round they are in through `BetContext::roundType`, and
  `LowRiskStrategy` bids 0 in them rather than reading a hand it is not supposed to see. **This
  changes scores: a game played on 3.0.0 does not reproduce on 4.0.0.** It is the only intentional
  behaviour change in this release.
- Every accessor whose answer depends on a current round now throws `std::logic_error` before
  `start()`, instead of indexing an empty vector. `isSetUp()` is how to ask.
- The engine validates the moves it is given — an out-of-range bid, a barred bid, an index that is
  not a card in the hand — and throws rather than obeying. In 3.x the bundled strategies happened
  to be well-behaved and nothing checked.

### Fixed

- **A completed round no longer misreports itself.** Every finished `Round` held its trump and its
  tricks as `Card*` into a deck that is re-shuffled at the top of each round, so from round two
  onward every stored round reported cards that were never played in it — and kept changing for
  the rest of the game. Nothing read round history, so nobody had seen it; the first client to
  render "the previous round" would have. Cards are held by value now.

### Removed

- `Bet::actual`, `Round::setResult()`, `Round::getActual()`, `Round::hasBet()`. Tricks won are
  derived from the tricks the round already stores.
- `GameEngine::getPlayer(Seat)`, and the rest of the driving API listed in the migration table.

## 3.0.0 and earlier

Predate this changelog. See the git history.
