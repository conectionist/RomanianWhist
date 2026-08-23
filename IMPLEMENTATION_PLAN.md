# Romanian Whist Implementation Plan

## Goal

Turn the current terminal prototype into a playable Romanian Whist game with clear rules, reliable game state, scoring, and a terminal UI that can run a full match from setup to final standings.

## Current State

The project already has the main domain objects:

- `Card`: rank/suit representation and display.
- `Deck`: stores cards and exposes indexed access.
- `Player`: stores name, hand, and next-player pointer.
- `Trick`: represents one trick.
- `Round`: stores trick count, trump card, first player, round type, bets, and results.
- `Scoreboard`: creates the round schedule.
- `GameEngine`: owns players, deck, scoreboard, and game status.
- `TerminalRomanianWhist`: handles setup and the terminal game loop.

Important gaps:

- Deck shuffling is not implemented.
- Player betting always returns `0`.
- Player turn order is not wired.
- The game loop does not play tricks.
- Trick winner logic is missing.
- Score calculation is missing.
- Rounds never advance.
- The game never reaches `Finished`.
- `8-1-8` is listed but not implemented.
- Several classes store raw pointers into vectors, which needs careful cleanup or replacement.

## Rule Decisions To Confirm

Before implementing scoring and round validation, decide these house rules:

1. Supported player count:
   - Confirmed: support 2-6 players.
   - Two-player Romanian Whist is valid, even if it is less fun than games with more players.

2. Deck size by player count:
   - Confirmed: use two ranks per player, across all four suits.
   - 2 players: Ace, King, Queen, Jack.
   - 3 players: Ace, King, Queen, Jack, 10, 9.
   - 4 players: Ace, King, Queen, Jack, 10, 9, 8, 7.
   - 5 players: Ace through 5.
   - 6 players: Ace through 3.

3. Round structure:
   - Confirmed: in both structures, the 1-card rounds and 8-card rounds repeat once per player.
   - `1-8-1`: repeated 1-card rounds, then 2 through 7, repeated 8-card rounds, then 7 through 2, then repeated 1-card rounds.
   - `8-1-8`: the reverse of `1-8-1`: repeated 8-card rounds, then 7 through 2, repeated 1-card rounds, then 2 through 7, then repeated 8-card rounds.
   - 4-player `1-8-1` example: `1,1,1,1,2,3,4,5,6,7,8,8,8,8,7,6,5,4,3,2,1,1,1,1`.
   - 4-player `8-1-8` example: `8,8,8,8,7,6,5,4,3,2,1,1,1,1,2,3,4,5,6,7,8,8,8,8`.

4. Trump rules:
   - Confirmed: this applies for all player counts and deck sizes.
   - For rounds below 8 cards, turn the next deck card as trump.
   - For 8-card rounds, there is no trump.

5. Bidding restriction:
   - Confirmed: the final bidder is forbidden from making the total bids equal the number of tricks in the round.
   - The last player's valid bid range is still `0` through the number of cards in the round, except for the one value that would make the table's total bid equal the round hand count.

6. Play restrictions:
   - Confirmed: legal play follows a strict priority order.
   - First priority: if the player has the lead suit, they must play the lead suit.
   - Second priority: if the player has no lead-suit card but has trump, they must play trump.
   - Third priority: if the player has neither the lead suit nor trump, they may play anything.
   - Overtrumping is not required. When a player must play trump, they do not have to beat an already-played trump card.
   Observation: if a player has both the lead suit and a trump, the player must play the lead. they cannot choose between either playing the lead suit or the trump.

7. Scoring:
   - Confirmed base scoring:
     - Exact bid scores `5 + bid`.
     - Missed bid scores `-abs(bid - actual)`.
   - Confirmed streak bonuses and penalties:
     - If a player wins 5 consecutive rounds, excluding 1-card rounds, they receive a bonus of 10 extra points on the 5th round in addition to the normal round score.
     - If a player loses 5 consecutive rounds, excluding 1-card rounds, they receive a penalty of 10 extra points on the 5th round in addition to the normal round loss.
   - For implementation: streak counters should ignore 1-card rounds entirely.

8. Special rounds:
   - `Forehead`: the player cannot see their own card before betting, but all other players can see it.
   - `Hidden`: nobody can see the player's card before betting, including the player and all other players. Everybody bets blindly.
   - Confirmed: whether all 1-card games are forehead rounds is controlled by the `all1GamesAreForehead` flag chosen by the user at the beginning of the game.
   - Current implementation note: `all1GamesAreForehead` is already passed into `Scoreboard::initialize`, and every 1-card round is marked as `RoundType::Forehead` when the flag is true.
   - Current implementation note: `endWithForeheadAndHidden` already appends one final forehead round and one final hidden round.

## Implementation Phases

### Phase 1: Stabilize The Core Model

- Add stable player identity instead of relying on names or raw vector addresses.
- Replace fragile `Player*` ownership patterns where practical.
- Ensure player order can be iterated safely from any starting player.
- Decide whether `GameEngine` or `Scoreboard` is the source of truth for current round state.
- Make getters const-correct where useful.
- Avoid copying players when round state needs references to the live player list.

Expected result:

- The engine can safely identify players, first player, next player, and round participants.

### Phase 2: Deck And Dealing

- Implement deck initialization for supported player counts.
- Implement real shuffling.
- Reset or rebuild the deck cleanly before each round.
- Deal cards in correct player order.
- Set trump card when applicable.
- Expose each player's hand for terminal display and card selection.

Expected result:

- A round can begin with shuffled cards, valid hands, and the correct trump/no-trump state.

### Phase 3: Round Schedule

- Complete `Scoreboard::initialize` for both `1-8-1` and `8-1-8`.
- Store round type for normal, forehead, and hidden rounds.
- Validate round count and first-player rotation.
- Add safe current-round advancement.
- Detect when no rounds remain.

Expected result:

- The scoreboard can generate the complete match structure and advance through it.

### Phase 4: Terminal Setup

- Re-enable real setup instead of `initializeTest`.
- Validate player count input.
- Validate unique player names if names remain user-facing identifiers.
- Validate game structure selection.
- Validate special-round options.
- Add a clear summary before starting the match.

Expected result:

- A user can configure a match from the terminal without editing code.

### Phase 5: Betting

- Replace `Player::getBet()` stub with terminal input handled by `TerminalRomanianWhist` or a UI-facing input helper.
- Validate bets are between `0` and cards in the round.
- Apply final-bidder restriction if enabled.
- Store each player's bid in the current round.
- Display all bids before card play begins.

Expected result:

- Each round records valid bets from all players in the correct order.

### Phase 6: Card Play

- Let each player choose a card from their hand.
- Validate card choice.
- Track lead suit for each trick.
- Enforce follow-suit/trump rules.
- Remove played cards from player hands.
- Store played cards in `Trick`.
- Rotate trick order based on previous trick winner.

Expected result:

- All tricks in a round can be played legally from terminal input.

### Phase 7: Trick Winner Logic

- Implement comparison rules:
  - Highest trump wins if any trump was played.
  - Otherwise highest card of the lead suit wins.
- Track each player's won-trick count.
- Set trick winner.
- Set next trick's first player to the trick winner.

Expected result:

- The engine can determine every trick winner and accumulate round results.

### Phase 8: Scoring

- Store actual tricks won per player at the end of each round.
- Implement scoring based on confirmed house rules.
- Maintain cumulative scores across rounds.
- Display round score and total score after each round.
- Display final ranking at the end of the match.

Expected result:

- A complete game produces a correct scoreboard and final winner.

### Phase 9: Special Rounds

- Implement forehead round display rules.
- Implement hidden round display rules after confirming exact behavior.
- Ensure betting and play still use the same validation path.
- Add clear terminal prompts for special rounds.

Expected result:

- Optional special one-card rounds work consistently with the normal game.

### Phase 10: Tests And Verification

- Add a build command or simple `Makefile`.
- Add unit tests for:
  - Deck composition by player count.
  - Round schedule generation.
  - Legal card validation.
  - Trick winner calculation.
  - Scoring.
  - Round advancement and game completion.
- Add at least one scripted integration test for a small deterministic match.

Expected result:

- Core game rules can be verified without playing manually every time.

## Suggested Implementation Order

1. Fix player identity and turn order.
2. Implement deck shuffle and per-round reset.
3. Finish round schedule and advancement.
4. Restore real terminal initialization.
5. Implement betting input and validation.
6. Implement card choice and legal-play validation.
7. Implement trick winner calculation.
8. Implement scoring.
9. Add final scoreboard and game completion.
10. Add special rounds.
11. Add tests and build tooling.

## Design Notes

- Keep UI input/output in `TerminalRomanianWhist`.
- Keep rule decisions in `GameEngine` or dedicated rule helpers, not scattered through terminal prompts.
- Prefer value-based identifiers or stable indexes over raw `Player*` stored across vector copies.
- Avoid using player name as the only key for score data unless names are validated as unique.
- Keep `Round` focused on round state, not terminal behavior.
- Keep `Scoreboard` focused on schedule and scores.

## Definition Of Done

The implementation is complete when:

- A user can start the program, configure a match, and play every round.
- Every player can bid and play cards legally.
- Tricks and round winners are computed correctly.
- Scores update after every round.
- The match ends automatically after the final round.
- Final standings are displayed.
- The code has basic automated coverage for core rules.
