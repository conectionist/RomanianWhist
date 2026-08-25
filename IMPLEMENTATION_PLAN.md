# Romanian Whist Implementation Plan

## Goal

Turn the current terminal prototype into a playable Romanian Whist game with clear rules, reliable game state, scoring, and a terminal UI that can run a full match from setup to final standings.

## Current State

The game already runs end-to-end in AI-vs-AI mode. The following are fully implemented:

- `Card`: rank/suit representation and display.
- `Deck`: initialization by player count, real shuffling.
- `Player`: name, hand, score, streak counters, `IMoveProvider` delegation.
- `Trick`: lead suit, played cards, winner.
- `Round`: trick count, trump card, first player, round type, bets, and results.
- `Scoreboard`: generates the complete round schedule for both `1-8-1` and `8-1-8`, advances rounds, detects game end.
- `GameEngine`: owns players, deck, scoreboard; wires dealing, betting, card play, trick winner logic, scoring.
- `TerminalRomanianWhist`: full game loop — shuffle, deal, betting, trick play, scoring, final standings.
- `CardValidator`: enforces follow-suit → trump → anything priority.
- Bidding validation: range `[0, trick count]`, plus the final-bidder restriction via `GameEngine::getForbiddenBet()` / `isBetLegal()`, carried to players in `BetContext::forbiddenBet`.
- Trick winner logic: highest trump wins; otherwise highest lead-suit card wins.
- Scoring: `5 + bid` on exact, `-abs(bid - actual)` on miss, streak bonuses/penalties at 5 consecutive wins/losses (excluding 1-card rounds).

Remaining gaps:

- `initialize()` is disabled; the game starts with a hardcoded test setup.
- `RoundType::Forehead` and `RoundType::Hidden` are stored in the schedule but the UI ignores them.
- No automated tests.

## Rule Decisions (All Confirmed)

1. **Player count**: 2–6 players.

2. **Deck size**: two ranks per player, across all four suits.
   - 2 players: Ace, King, Queen, Jack.
   - 3 players: Ace through 9.
   - 4 players: Ace through 7.
   - 5 players: Ace through 5.
   - 6 players: Ace through 3.

3. **Round structure**: in both structures, the 1-card and 8-card rounds repeat once per player.
   - `1-8-1`: repeated 1-card rounds, 2–7, repeated 8-card rounds, 7–2, repeated 1-card rounds.
   - `8-1-8`: repeated 8-card rounds, 7–2, repeated 1-card rounds, 2–7, repeated 8-card rounds.

4. **Trump**: for rounds below 8 cards, turn the next deck card as trump. For 8-card rounds, no trump.

5. **Bidding restriction**: the final bidder cannot make the total bids equal the number of tricks in the round.

6. **Play restrictions** (priority order):
   - Must follow lead suit if possible.
   - Must play trump if no lead-suit card is held.
   - May play anything if neither lead suit nor trump is held.
   - Overtrumping is not required.

7. **Scoring**:
   - Exact bid: `5 + bid`.
   - Missed bid: `-abs(bid - actual)`.
   - Streak bonus: +10 on the 5th consecutive win (excluding 1-card rounds).
   - Streak penalty: -10 on the 5th consecutive loss (excluding 1-card rounds).

8. **Special rounds**:
   - `Forehead`: the active player cannot see their own card before betting; all others can.
   - `Hidden`: nobody can see the active player's card before betting.
   - `all1GamesAreForehead` flag marks all 1-card rounds as forehead.
   - `endWithForeheadAndHidden` appends one forehead and one hidden round at the end.

## Remaining Implementation

### Phase 9: Special Rounds

- During a **Forehead** round: show all other players' cards to everyone, but hide the active player's card from themselves before betting.
- During a **Hidden** round: hide the active player's card from all players before betting.
- Add clear terminal prompts identifying forehead and hidden rounds.
- Ensure betting and play validation still use the same path as normal rounds.

Expected result:

- Optional special one-card rounds work consistently with the normal game.

### Phase 10: Tests And Verification

- Add a `Makefile` or build script.
- Add unit tests for:
  - Deck composition by player count.
  - Round schedule generation for both `1-8-1` and `8-1-8`.
  - Legal card filtering (`CardValidator`).
  - Trick winner calculation.
  - Scoring, including streak bonuses and penalties.
  - Round advancement and game completion detection.
- Add at least one scripted integration test for a small deterministic match.

Expected result:

- Core game rules can be verified without playing manually every time.

## Design Notes

- Keep UI input/output in `TerminalRomanianWhist`.
- Keep rule decisions in `GameEngine` or dedicated rule helpers, not scattered through terminal prompts.
- Player name is currently used as the key in `Round::bets`. Ensure names are validated as unique in Phase 4.
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
