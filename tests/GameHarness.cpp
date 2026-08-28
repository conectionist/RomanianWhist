#include "GameHarness.h"
#include "TestSupport.h"

#include <romanian_whist/Trick.h>

#include <stdexcept>

namespace romanian_whist::test
{
GameEngine playFullGame(GameStructure structure,
                        std::vector<std::unique_ptr<IMoveProvider>> providers,
                        std::uint32_t seed,
                        const GameHooks& hooks)
{
    GameEngine game(seed);

    for(std::size_t i = 0 ; i < providers.size() ; i++)
        game.addPlayer("P" + std::to_string(i), std::move(providers[i]));

    game.initializeScoreboard(structure, false, false);
    game.initializeDeck(static_cast<unsigned int>(providers.size()));
    game.setStatus(GameStatus::InProgress);

    while(game.isInProgress())
    {
        game.shuffleDeck();
        game.dealCards();

        auto currentPlayer = game.getFirstPlayerOfTheRound();

        for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
        {
            const unsigned int bet = currentPlayer->getBet(game.getCurrentTrumpCard(),
                                                           i == 0,
                                                           game.getForbiddenBet());

            if(hooks.onBeforeBetPlaced)
                hooks.onBeforeBetPlaced(game, seatOf(game, currentPlayer), bet);

            game.placeBet(currentPlayer, bet);
            currentPlayer = game.getNextPlayer(currentPlayer);
        }

        // Redundant but harmless: Round::getActual() already returns 0 for a
        // seat with no result recorded yet, so this just makes that explicit
        // before the trick loop starts writing real values.
        currentPlayer = game.getFirstPlayerOfTheRound();
        for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
        {
            game.setResult(currentPlayer, 0);
            currentPlayer = game.getNextPlayer(currentPlayer);
        }

        const unsigned int trickCount = game.getCurrentRoundTrickCount();
        std::vector<unsigned int> tricksWon(game.getPlayerCount(), 0);

        for(unsigned int trickIndex = 0 ; trickIndex < trickCount ; trickIndex++)
        {
            Trick trick;

            const auto leader = game.getFirstPlayerOfTheRound();
            currentPlayer = leader;

            for(unsigned int i = 0 ; i < game.getPlayerCount() ; i++)
            {
                Card* trump = game.getCurrentTrumpCard();
                const Suit* leadSuit = trick.hasLeadSuit() ? &trick.getLeadSuit() : nullptr;

                const std::vector<Card*> handBeforePlay = currentPlayer->getHand();

                Card* playedCard = currentPlayer->playCard(trump,
                                                           leadSuit,
                                                           trick.getPlayedCards(),
                                                           game.getCurrentRound().getBet(currentPlayer->getName()),
                                                           tricksWon[seatOf(game, currentPlayer)]);

                if(playedCard == nullptr)
                    throw std::runtime_error(currentPlayer->getName() + " had no legal card to play.");

                if(hooks.onBeforeCardPlayed)
                    hooks.onBeforeCardPlayed(handBeforePlay, trump, leadSuit, playedCard);

                if(!trick.hasLeadSuit())
                    trick.setLeadSuit(playedCard->suit);

                trick.addPlayedCard(playedCard);

                currentPlayer = game.getNextPlayer(currentPlayer);
            }

            const auto winner = game.determineTrickWinner(trick, leader);
            trick.setWinner(winner);
            game.addTrickToCurrentRound(trick);
            game.setFirstPlayerOfTheRound(winner);

            const unsigned int winnerSeat = seatOf(game, winner);
            tricksWon[winnerSeat]++;
            game.setResult(winner, tricksWon[winnerSeat]);
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

        for(unsigned int i = 0 ; i < players.size() ; i++)
        {
            const std::string& name = players.at(i).getName();
            row.emplace_back(g.getCurrentRound().getBet(name), g.getCurrentRound().getActual(name));
        }

        record.push_back(row);
    };
}

} // namespace romanian_whist::test
