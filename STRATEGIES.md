# AI strategies

The engine ships four `IStrategy` implementations. This document covers what each one
does, how they compare, and where they fall down. For the interface itself, see
[README.md](README.md#connecting-your-interface).

| Strategy | Header | Bids | Plays |
|---|---|---|---|
| [`RandomCardStrategy`](#randomcardstrategy) | `strategies/RandomCardStrategy.h` | A random legal number | A random legal card |
| [`FirstCardStrategy`](#firstcardstrategy) | `strategies/FirstCardStrategy.h` | 0, or 1 when 0 is barred | The first legal card in hand order |
| [`DuckingStrategy`](#duckingstrategy) | `strategies/DuckingStrategy.h` | 0, come what may | Sheds its highest cards whenever they cannot win |
| [`LowRiskStrategy`](#lowriskstrategy) | `strategies/LowRiskStrategy.h` | The tricks its hand will take anyway — usually 0 | Wins cheaply while it owes tricks, then ducks |

---

## Why they all want to bid zero

The scoring rule shapes every decision here. From `Scoreboard::calculateRoundScore`:

```
hit your bid   ->  +5 + bid
miss your bid  ->  -|bid - actual|
```

plus a ±10 swing for five consecutive hits or misses.

Two things follow. First, **the reward for accuracy dwarfs the reward for ambition**: bidding
0 and taking 0 scores +5, while bidding 3 and taking 3 scores +8. Tripling your bid buys
you 60% more points for far more risk. Second, **missing is cheap** — usually just -1. A
strategy that quietly hits a small bid every round beats one that occasionally hits a big
one, and the streak bonus rewards it again for the consistency.

So "aim for zero" is not timidity, it is what the scoring asks for.

---

## RandomCardStrategy

The baseline. Bids uniformly over `[0, hand.size()]`, drawing from a range one short and
stepping over `*forbiddenBet` so the result is uniform across what is legal without
rejecting and redrawing. Plays a uniformly random legal card.

Useful as a control when measuring another strategy, and as a stand-in for an unpredictable
opponent. It seeds a fresh `std::random_device` on every call, so it is not reproducible
and cannot be unit tested as written.

## FirstCardStrategy

Bids 0, or 1 when the final-bidder rule bars 0. Plays `legalCards[0]` — the first legal
card in hand order, which is deal order, so it is arbitrary but deterministic.

It bids as well as `DuckingStrategy` does and still scores half as much, which makes it a
clean measurement of what the play alone is worth. Its problem is that hand order
correlates with nothing: it will drop an ace under a seven and then be stuck holding a
king it can no longer get rid of safely.

## DuckingStrategy

Bids 0 and means it. Never chases a trick, not even one it has been forced to bid for —
when 0 is barred it bids 1, takes the resulting -1, and carries on ducking.

Play is `chooseDuckingCard` (below) on every trick. The single-minded version of the idea,
and the one to reach for when you want a predictable opponent.

Worth knowing: **0 is almost never actually barred**. `getForbiddenBet()` returns 0 only
when the earlier bids already add up to exactly the trick count, which is rare — in a
four-ducker table it never happened once in 200 games. The usual barred value is the trick
count itself, not 0. So this strategy's "come what may" is less of a sacrifice than it
sounds.

## LowRiskStrategy

Counts the tricks its hand will take whether it wants them or not, bids exactly that, then
plays to the bid.

**Bidding** — `countLikelyWinners` is deliberately crude:

```
winners  = (aces outside the trump suit)
         + (trump cards ranked King or Ace)
if trump length > 2:
    winners += trump length - 2      // the tail of a long trump holding wins by force
return min(winners, hand.size())
```

In a no-trump round (`trump == nullptr`, the 8-card rounds) only aces count.

If that number is the one the final-bidder rule bars, it steps **down** rather than up.
Shedding one trick it expected to win is something a ducking hand can usually manage;
forcing an extra trick out of a hand that has none is a coin flip. Stepping down from 0 is
not possible, so 0 becomes 1 — and a round always has at least one trick, so 1 is safe.

**Playing** — while `tricksWon < bet` it uses `chooseWinningCard`; once the bid is met or
overshot, every further trick is a penalty, so it switches to `chooseDuckingCard` for the
rest of the round.

---

## The shared machinery

Both low-risk strategies are about a dozen lines each, because the judgement calls live in
`strategies/TrickHeuristics.h`. Reuse it rather than re-deriving any of this.

### What makes "safe" exact

**A card that does not beat the trick's current winner cannot take the trick** — no matter
how many players are still to come, since they can only push the winner higher.

That turns "is this risky?" from a probability estimate into a lookup. No opponent
modelling, no counting seats, no card tracking. It is the reason these strategies are short.

The ranking comes from `CardValidator::beats` and `CardValidator::getWinningCard`, which
are the same functions `GameEngine::determineTrickWinner` uses. A strategy asking "would
this win?" reasons with the very rule that will later declare the winner, so the two cannot
drift apart.

### Danger ordering

`isMoreDangerous` ranks cards by how much trouble they are to a player who would rather not
win: **any trump outranks any plain card**, and rank decides within that. A low trump is
more dangerous than a high plain card, because it takes tricks its rank alone would never
justify.

Everything else is built from that ordering:

| Function | When leading | When following |
|---|---|---|
| `chooseDuckingCard` | Nothing is safe, so lead the **least** dangerous card | Dump the **most** dangerous card that still cannot win. If every legal card would win, take the trick with the **least** dangerous one — cheapest possible, and a later player may still overtake |
| `chooseWinningCard` | Lead the **most** dangerous card | Take it with the **least** dangerous card that wins. If none can win, discard the **least** dangerous and keep the good ones for a trick still worth having |

The ducking case is the one that matters. Concretely, holding J♥ and 9♥:

- **Q♥ led** — the jack cannot beat the queen, so it is safe. Shed the jack.
- **10♥ led** — the jack *would* win. Duck with the nine and keep the jack for a trick where
  something bigger has already been played.

---

## Measured behaviour

200 full 1-8-1 games per line-up, four bots, played headless. "tricks/round" is pinned by
arithmetic: the four seats always share out the round's tricks between them, so across the
1-8-1 schedule the table average is ~0.92 whatever anyone does. Only the spread is a
strategy's doing.

**One of each**

| seat | avg score | hit bid | bid 0 | tricks/round |
|---|---|---|---|---|
| LowRisk | **60.6** | 54.8% | 54.2% | 0.96 |
| Ducking | 40.5 | 50.2% | 93.6% | 0.76 |
| FirstCard | 21.8 | 43.8% | 93.1% | 1.00 |
| Random | 0.8 | 33.0% | 35.5% | 0.97 |

Ducking and FirstCard bid zero about equally often, so the 19-point gap between them is
the *play* alone.

**LowRisk against three Random** — 65.0 against -6.5, -5.3, -7.6.

**LowRisk against three Ducking** — 53.7 against roughly 33. Its trick count rises to 1.08
per round: the duckers are all shedding, so somebody has to take those tricks, and LowRisk
is the only one at the table willing to.

**Mirror matches** are where it gets interesting:

| line-up | avg score each | hit bid | bid 0 |
|---|---|---|---|
| Four LowRisk | ~65 | 57% | 56% |
| Four Ducking | ~30 | 48% | **100%** |

Four duckers all bid 0, and the tricks still have to go somewhere — so they collectively
guarantee that most of them miss. Four LowRisk bots score more than twice as much against
each other, because bidding for the tricks you are going to be handed anyway is the whole
point. **The ducking strategy is parasitic: it needs someone else at the table willing to
take tricks.**

---

## Known weaknesses

- **`countLikelyWinners` over-bids.** LowRisk bids 0 only ~55% of the time against
  Ducking's ~94%, and the `trump length - 2` term is the most suspect part. It still scores
  best, so it has been left alone — but it is the first knob to turn.
- **No memory.** Neither strategy tracks which cards have been played in earlier tricks, so
  it cannot know its king has become the highest card left in a suit. `PlayContext` carries
  only the current trick.
- **Leading is guesswork.** With no card tracking there is no way to know whether a low
  card is actually safe to lead, so `chooseDuckingCard` just leads its smallest and hopes.
- **No opponent modelling.** Bids are visible via `Round::getBet`, but nothing reads them.
  A bot could duck far more confidently knowing the player behind it has bid 0 too.
- **Ducking needs a victim**, as the mirror match above shows.

---

## Writing your own

Implement `IStrategy` and add the source to the library's explicit source list in
`CMakeLists.txt`.

```cpp
#include <romanian_whist/strategies/IStrategy.h>
#include <romanian_whist/strategies/TrickHeuristics.h>

class MyStrategy : public romanian_whist::IStrategy
{
public:
    unsigned int getBestBet(const romanian_whist::BetContext& context) override
    {
        // Honour *context.forbiddenBet whenever it is set and the bid is legal.
        return 0;
    }

    romanian_whist::Card* getBestChoice(const romanian_whist::PlayContext& context) override
    {
        // cardValidator is inherited from IStrategy.
        const auto legal = cardValidator.getLegalCards(context.hand, context.trump, context.leadSuit);

        if(legal.empty())
            return nullptr;

        return romanian_whist::heuristics::chooseDuckingCard(context, legal);
    }
};
```

Three things to get right:

1. **Return a pointer from `context.hand`,** never a copy. `Player::playCard` removes the
   played card by identity.
2. **Handle `trump == nullptr`.** The 8-card rounds have no trump.
3. **Guard the empty case** and return `nullptr`, which callers may treat as an error.

Both `getBestBet` and `getBestChoice` are pure functions of their arguments, so a
deterministic strategy can be tested without standing up a `GameEngine` at all.
