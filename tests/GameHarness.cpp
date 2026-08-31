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
    auto enginePtr = std::make_unique<GameEngine>();
    GameEngine& game = *enginePtr;

    // Registered before start(), because start() is what fires onGameStarted().
    for(IGameObserver* observer : observers)
        game.addObserver(observer);

    game.start(buildSetup(structure, std::move(providers), seed,
                          endWithForeheadAndHidden, all1GamesAreForehead));
    game.run();

    return enginePtr;
}

GameSetup buildSetup(GameStructure structure,
                     std::vector<std::unique_ptr<IMoveProvider>> providers,
                     std::uint32_t seed,
                     bool endWithForeheadAndHidden,
                     bool all1GamesAreForehead)
{
    GameSetup setup;

    setup.structure = structure;
    setup.endWithForeheadAndHidden = endWithForeheadAndHidden;
    setup.all1GamesAreForehead = all1GamesAreForehead;
    setup.shuffleSeed = seed;

    for(std::size_t i = 0 ; i < providers.size() ; i++)
        setup.seats.push_back(SeatSetup{ "P" + std::to_string(i), std::move(providers[i]) });

    return setup;
}

std::vector<int> finalScores(const GameEngine& engine)
{
    std::vector<int> scores;

    for(unsigned int i = 0 ; i < engine.getPlayerCount() ; i++)
        scores.push_back(engine.getTotalScore(Seat{i}));

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
