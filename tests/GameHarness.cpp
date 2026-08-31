#include "GameHarness.h"

namespace romanian_whist::test
{
std::unique_ptr<GameEngine> playFullGame(GameStructure structure,
                                         std::vector<std::unique_ptr<IMoveProvider>> providers,
                                         std::uint32_t seed,
                                         const std::vector<IGameObserver*>& observers,
                                         bool endWithForeheadAndHidden,
                                         bool all1GamesAreForehead)
{
    auto enginePtr = std::make_unique<GameEngine>(seed);
    GameEngine& game = *enginePtr;

    const std::size_t playerCount = providers.size();

    for(std::size_t i = 0 ; i < playerCount ; i++)
        game.addPlayer("P" + std::to_string(i), std::move(providers[i]));

    game.initializeScoreboard(structure, endWithForeheadAndHidden, all1GamesAreForehead);
    game.initializeDeck(static_cast<unsigned int>(playerCount));

    // Registered before the game starts, because setStatus() is what fires
    // onGameStarted().
    for(IGameObserver* observer : observers)
        game.addObserver(observer);

    game.setStatus(GameStatus::InProgress);
    game.run();

    return enginePtr;
}

std::vector<int> finalScores(const GameEngine& engine)
{
    std::vector<int> scores;
    const auto& players = engine.getPlayers();

    for(unsigned int i = 0 ; i < players.size() ; i++)
        scores.push_back(players.at(i).getTotalScore());

    return scores;
}

void RoundRecorder::onRoundScored(const GameEngine& engine)
{
    // onRoundScored, not onRoundComplete: this is the last callback at which
    // getCurrentRoundIndex() still names the round being reported, so it is the
    // only one at which the round's own bets and results are still readable.
    std::vector<std::pair<unsigned int, unsigned int>> row;

    for(unsigned int i = 0 ; i < engine.getPlayerCount() ; i++)
    {
        const Seat seat{i};
        row.emplace_back(engine.getBet(seat).value_or(0), engine.getTricksWon(seat));
    }

    record.push_back(row);
}

} // namespace romanian_whist::test
