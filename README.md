# Romanian Whist Engine

A reusable C++20 game engine for [Romanian Whist](https://en.wikipedia.org/wiki/Romanian_Whist).
It contains all the game logic — dealing, betting, trick resolution, scoring — and **no
user interface**. You supply the UI by implementing a single interface.

The engine ships with AI opponents, so any client gets computer players for free.

---

## Requirements

- A C++20 compiler
- CMake 3.14 or newer

## Adding the engine to your project

### Step 1 — add it as a git submodule

From the root of your project:

```bash
git submodule add https://github.com/conectionist/romanian_whist_engine.git libs/RomanianWhistEngine
git commit -m "Add Romanian Whist engine as a submodule"
```

This clones the engine into `libs/RomanianWhistEngine/` and records the exact commit
your project depends on.

### Step 2 — wire it into your CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(MyWhistGame LANGUAGES CXX)

add_subdirectory(libs/RomanianWhistEngine)

add_executable(my_whist_game src/main.cpp)
target_link_libraries(my_whist_game PRIVATE RomanianWhist::engine)
```

That is all the wiring required. The include directory comes through automatically, so
you do **not** need a `target_include_directories` call of your own.

### Step 3 — include the headers

```cpp
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/strategies/RandomCardStrategy.h>
```

### Cloning a project that uses the engine

A plain `git clone` leaves submodule directories empty. Use either:

```bash
git clone --recurse-submodules <your-project-url>
```

or, if you already cloned without it:

```bash
git submodule update --init --recursive
```

### Updating to a newer version of the engine

```bash
cd libs/RomanianWhistEngine
git pull origin master
cd ../..
git add libs/RomanianWhistEngine
git commit -m "Update Romanian Whist engine"
```

---

## Building the engine on its own

```bash
cmake --preset default
cmake --build build
```

This produces `build/libromanian_whist_engine.a`.

---

## Usage

Everything lives in the `romanian_whist` namespace.

### Connecting your interface

The engine never reads input or prints output. It asks for player decisions through
`IMoveProvider`, which is the single seam between the engine and your UI:

```cpp
namespace romanian_whist {

class IMoveProvider
{
public:
    virtual ~IMoveProvider() = default;

    // How many tricks does this player bet they will win?
    virtual unsigned int makeBet(const BetContext& context) = 0;

    // Which card does this player play? Return one of the pointers from
    // `context.hand`; the engine removes it for you.
    virtual Card* playCard(const PlayContext& context) = 0;
};

}
```

`BetContext` carries everything the bid decision needs:

```cpp
struct BetContext
{
    const std::vector<Card*>& hand;              // size == the round's trick count
    Card* trump = nullptr;                       // null in 8-card rounds
    bool isFirstPlayer = false;                  // opens the bidding
    std::optional<unsigned int> forbiddenBet;    // see below
};
```

`PlayContext` does the same for the card decision:

```cpp
struct PlayContext
{
    const std::vector<Card*>& hand;
    const std::vector<Card*>& playedCards;       // this trick, in play order
    Card* trump = nullptr;                       // null in 8-card rounds
    const Suit* leadSuit = nullptr;              // null when leading
    unsigned int bet = 0;                        // this player's bid this round
    unsigned int tricksWon = 0;                  // of it, so far
};
```

`playedCards` is what makes it possible to ask whether a card would actually win:
pass it to `CardValidator::getWinningCard`, then `CardValidator::beats`. A card that
does not beat the current winner cannot take the trick however many players are
still to go, since they can only push the winner higher.

`forbiddenBet` is the bidding restriction: the final bidder may not make the
round's bids add up to exactly the trick count. It is set only for that bidder,
and only while a bid could still hit the total, so **an implementation that
honours it whenever it is present is always making a legal bid.** Everything
else in range `[0, hand.size()]` is fair game.

Implement it once for your UI, then hand one instance to each human player:

```cpp
#include <romanian_whist/IMoveProvider.h>
#include <romanian_whist/CardValidator.h>

class MyUiMoveProvider : public romanian_whist::IMoveProvider
{
public:
    unsigned int makeBet(const romanian_whist::BetContext& context) override
    {
        // Re-prompt while the answer equals *context.forbiddenBet.
        return askUserForBet(context);
    }

    romanian_whist::Card* playCard(const romanian_whist::PlayContext& context) override
    {
        // getLegalCards applies the rules of the game for you.
        romanian_whist::CardValidator validator;
        auto legal = validator.getLegalCards(context.hand, context.trump, context.leadSuit);

        // Return nullptr if `legal` is empty - the built-in strategies do.
        return askUserToPick(legal);
    }
};
```

> Use `CardValidator::getLegalCards` rather than writing your own rule checks — it
> enforces following suit and trumping correctly.

### Setting up a game

```cpp
#include <romanian_whist/GameEngine.h>
#include <romanian_whist/AiMoveProvider.h>
#include <romanian_whist/strategies/RandomCardStrategy.h>

using namespace romanian_whist;

GameEngine game;

game.addPlayer("You", std::make_unique<MyUiMoveProvider>());
game.addPlayer("Bot", std::make_unique<AiMoveProvider>(
                          std::make_unique<RandomCardStrategy>()));

game.initializeScoreboard(GameStructure::S_181,
                          /* endWithForeheadAndHidden */ true,
                          /* all1GamesAreForehead    */ false);
game.initializeDeck(2);
```

`GameStructure::S_181` runs 1-8-1; `GameStructure::S_818` runs 8-1-8.

### Driving the game loop

The engine does not own the loop — your client does:

```cpp
game.setStatus(GameStatus::InProgress);

while(game.isInProgress())
{
    game.shuffleDeck();
    game.dealCards();

    // 1. collect a bet from every player
    auto player = game.getFirstPlayerOfTheRound();
    for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
    {
        bool isFirst = player->getName() == game.getFirstPlayerOfTheRound()->getName();

        // Asked fresh each time: it only ever names a bid for the final bidder,
        // and only once everyone before them has bid.
        auto forbidden = game.getForbiddenBet();

        game.placeBet(player, player->getBet(game.getCurrentTrumpCard(), isFirst, forbidden));
        player = game.getNextPlayer(player);
    }

    // 2. play each trick (see Trick / determineTrickWinner)
    // 3. report results with game.setResult(player, tricksWon)

    game.calculateScores();     // per-round scores, readable before committing
    game.commitRoundScores();   // fold them into the totals
    game.completeCurrentRound();
}
```

`getCurrentTrumpCard()` returns `nullptr` in a game of 8 (no trump).

### Reading scores

```cpp
// name -> total score, ranked best first
for(const auto& [name, score] : game.getPlayerScores())
    display(name, score);

// name -> {round score, total score}
for(const auto& [name, scores] : game.getPlayerRoundScores())
    display(name, scores.first, scores.second);
```

### Reading live game state

`getPlayerScores()` is enough for a score table. A richer interface — one that
shows who bid what, who is taking tricks, or which round type is in play — reads
the game directly:

```cpp
// Where are we?
display(game.getCurrentRoundIndex() + 1, game.getRoundCount());  // "round 7 of 24"
display(game.getCurrentRoundTrickCount());                       // cards dealt this round
display(game.getCurrentRoundType());                             // Normal / Forehead / Hidden

// What has each seat done?
const romanian_whist::Round& round = game.getCurrentRound();

for(const auto& player : game.getPlayers())
{
    const std::string& name = player.getName();

    // getBet() returns 0 for a player who has not bid yet, which is
    // indistinguishable from a genuine bid of 0. Ask hasBet() first.
    if(round.hasBet(name))
        display(name, round.getBet(name), round.getActual(name));
    else
        display(name, "-");

    display(player.getHand().size(), player.getTotalScore());

    // Five exact bids in a row is worth +10, five misses -10.
    display(player.getConsecutiveWins(), player.getConsecutiveLosses());
}
```

`Round::getActual()` reflects whatever you last passed to
`GameEngine::setResult()`. Calling `setResult()` after every trick rather than
once at the end of the round keeps it live, which is what a running "tricks won"
column wants — the call is an assignment, not an increment, so repeating it is
safe.

Two accessors answer different questions about turn order, and mixing them up is
easy: `Round::getOpeningPlayer()` is who led the round's first trick and so
determines bidding order, fixed for the round. `Round::getFirstPlayer()` is who
leads the *next* trick, and is reassigned to each trick's winner as you call
`setFirstPlayerOfTheRound()`.

### Validating a bid

The engine owns the bidding restriction so that clients do not each have to
re-derive it:

```cpp
// The bid the final bidder may not make. Empty for every other bidder, and
// empty once the bids already exceed the trick count - no bid can hit the
// total then. Hand it to Player::getBet() and honouring it is enough.
std::optional<unsigned int> forbidden = game.getForbiddenBet();

// Or check a bid you already have, range included.
if(!game.isBetLegal(bet))
    reject(bet);
```

`placeBet()` records whatever it is given without judging it, so ask before you
call it. A legal bid is one in `[0, getCurrentRoundTrickCount()]` that is not
`*getForbiddenBet()`.

### Built-in AI strategies

`AiMoveProvider` delegates its decisions to an `IStrategy`:

| Strategy | Behaviour |
|---|---|
| `RandomCardStrategy` | Plays a random legal card; bids a random legal number of tricks |
| `FirstCardStrategy` | Plays the first legal card; bids 0, or 1 when 0 is barred |
| `LowRiskStrategy` | Bids the tricks its hand will take whether it wants them or not — usually 0 — then plays to that bid: takes tricks as cheaply as it can while it still owes some, and ducks every trick after that |
| `DuckingStrategy` | Bids 0 come what may and never chases a trick, dumping its highest cards at the moments they cannot win |

The two low-risk strategies share their judgement calls through
`<romanian_whist/strategies/TrickHeuristics.h>`, which is public — reuse it rather than
re-deriving "is this card safe to play?" for your own strategy.

Write your own by implementing `IStrategy` from
`<romanian_whist/strategies/IStrategy.h>`.

---

## Layout

```
include/romanian_whist/     public headers — this is the include root
├── GameEngine.h            main facade
├── IMoveProvider.h         implement this to supply a UI
├── BetContext.h            what IMoveProvider::makeBet is handed
├── PlayContext.h           what IMoveProvider::playCard is handed
├── CardValidator.h         legal-move rules and the trick ranking
├── AiMoveProvider.h        AI player driven by an IStrategy
└── strategies/             IStrategy + the bundled strategies
src/                        implementation
```

## Clients

- [romanian_whist_terminal](https://github.com/conectionist/romanian_whist_terminal) —
  terminal UI client
