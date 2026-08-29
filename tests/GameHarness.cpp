#include "GameHarness.h"

#include <romanian_whist/Player.h>
#include <romanian_whist/Trick.h>

#include <optional>
#include <stdexcept>

namespace romanian_whist::test
{
GameEngine playFullGame(GameStructure structure,
                        std::vector<std::unique_ptr<IMoveProvider>> providers,
                        std::uint32_t seed,
                        const GameHooks& hooks,
                        bool endWithForeheadAndHidden,
                        bool all1GamesAreForehead)
{
    GameEngine game(seed);

    for(std::size_t i = 0 ; i < providers.size() ; i++)
        game.addPlayer("P" + std::to_string(i), std::move(providers[i]));

    game.initializeScoreboard(structure, endWithForeheadAndHidden, all1GamesAreForehead);
    game.initializeDeck(static_cast<unsigned int>(providers.size()));
    game.setStatus(GameStatus::InProgress);

    while(game.isInProgress())
    {
        game.shuffleDeck();
        game.dealCards();

        Seat currentSeat = game.getRoundLeaderSeat();

        for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
        {
            const unsigned int bet = game.getPlayer(currentSeat).getBet(game.getCurrentTrumpCard(),
                                                                        i == 0,
                                                                        game.getForbiddenBet());

            if(hooks.onBeforeBetPlaced)
                hooks.onBeforeBetPlaced(game, currentSeat, bet);

            game.placeBet(currentSeat, bet);
            currentSeat = game.getNextSeat(currentSeat);
        }

        const unsigned int trickCount = game.getCurrentRoundTrickCount();

        for(unsigned int trickIndex = 0 ; trickIndex < trickCount ; trickIndex++)
        {
            Trick trick;

            currentSeat = game.getRoundLeaderSeat();

            for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
            {
                Player& player = game.getPlayer(currentSeat);

                Card* trump = game.getCurrentTrumpCard();
                const Suit* leadSuit = trick.hasLeadSuit() ? &trick.getLeadSuit() : nullptr;

                const std::vector<Card*> handBeforePlay = player.getHand();
                const std::vector<Card*> cardsSoFar = trick.cardsInPlayOrder();

                const std::optional<unsigned int> bet = game.getCurrentRound().getBet(currentSeat);

                Card* playedCard = player.playCard(trump,
                                                   leadSuit,
                                                   cardsSoFar,
                                                   bet.value_or(0),
                                                   game.getCurrentRound().getTricksWon(currentSeat));

                if(playedCard == nullptr)
                    throw std::runtime_error(player.getName() + " had no legal card to play.");

                if(hooks.onBeforeCardPlayed)
                    hooks.onBeforeCardPlayed(handBeforePlay, trump, leadSuit, playedCard);

                if(!trick.hasLeadSuit())
                    trick.setLeadSuit(playedCard->suit);

                trick.addPlayedCard(currentSeat, playedCard);

                currentSeat = game.getNextSeat(currentSeat);
            }

            const Seat winner = game.determineTrickWinner(trick);
            trick.setWinner(winner);
            game.addTrickToCurrentRound(trick);
            game.setRoundLeaderSeat(winner);
        }

        game.calculateScores();

        if(hooks.onRoundScored)
            hooks.onRoundScored(game);

        game.commitRoundScores();
        game.completeCurrentRound();
    }

    return game;
}

std::vector<int> finalScores(const GameEngine& engine)
{
    std::vector<int> scores;
    const auto& players = engine.getPlayers();

    for(unsigned int i = 0 ; i < players.size() ; i++)
        scores.push_back(players.at(i).getTotalScore());

    return scores;
}

std::function<void(const GameEngine&)> recordRoundsInto(RoundRecord& record)
{
    return [&record](const GameEngine& g)
    {
        std::vector<std::pair<unsigned int, unsigned int>> row;
        const auto& players = g.getPlayers();

        for(Seat seat = 0 ; seat < players.size() ; seat++)
        {
            const Round& round = g.getCurrentRound();
            row.emplace_back(round.getBet(seat).value_or(0), round.getTricksWon(seat));
        }

        record.push_back(row);
    };
}

} // namespace romanian_whist::test
