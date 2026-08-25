# Romanian Whist Engine

A reusable C++17 game engine for [Romanian Whist](https://en.wikipedia.org/wiki/Romanian_Whist).
It contains all the game logic — dealing, betting, trick resolution, scoring — and **no
user interface**. You supply the UI by implementing a single interface.

The engine ships with AI opponents, so any client gets computer players for free.

---

## Requirements

- A C++17 compiler
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
    virtual unsigned int makeBet(const std::vector<Card*>& hand,
                                 Card* trump,
                                 bool isFirstPlayer) = 0;

    // Which card does this player play? Remove it from `hand` before returning it.
    virtual Card* playCard(std::vector<Card*>& hand,
                           Card* trump,
                           const Suit* leadSuit) = 0;
};

}
```

Implement it once for your UI, then hand one instance to each human player:

```cpp
#include <romanian_whist/IMoveProvider.h>
#include <romanian_whist/CardValidator.h>

class MyUiMoveProvider : public romanian_whist::IMoveProvider
{
public:
    unsigned int makeBet(const std::vector<romanian_whist::Card*>& hand,
                         romanian_whist::Card* trump,
                         bool isFirstPlayer) override
    {
        return askUserForBet(hand, trump, isFirstPlayer);
    }

    romanian_whist::Card* playCard(std::vector<romanian_whist::Card*>& hand,
                                   romanian_whist::Card* trump,
                                   const romanian_whist::Suit* leadSuit) override
    {
        // getLegalCards applies the rules of the game for you.
        romanian_whist::CardValidator validator;
        auto legal = validator.getLegalCards(hand, trump, leadSuit);

        romanian_whist::Card* chosen = askUserToPick(legal);
        hand.erase(std::find(hand.begin(), hand.end(), chosen));
        return chosen;
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
        game.placeBet(player, player->getBet(game.getCurrentTrumpCard(), isFirst));
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

### Built-in AI strategies

`AiMoveProvider` delegates its decisions to an `IStrategy`:

| Strategy | Behaviour |
|---|---|
| `RandomCardStrategy` | Plays a random legal card; always bets 0 |
| `FirstCardStrategy` | Plays the first legal card; always bets 0 |

Write your own by implementing `IStrategy` from
`<romanian_whist/strategies/IStrategy.h>`.

---

## Layout

```
include/romanian_whist/     public headers — this is the include root
├── GameEngine.h            main facade
├── IMoveProvider.h         implement this to supply a UI
├── CardValidator.h         legal-move rules
├── AiMoveProvider.h        AI player driven by an IStrategy
└── strategies/             IStrategy + the bundled strategies
src/                        implementation
```

## Clients

- [romanian_whist_terminal](https://github.com/conectionist/romanian_whist_terminal) —
  terminal UI client
