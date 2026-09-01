include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(romanian_whist_engine_tests
    GameHarness.cpp
    CancellationTests.cpp
    MoveValidationTests.cpp
    CardValidatorTests.cpp
    DeckTests.cpp
    GameEngineTests.cpp
    ObserverTests.cpp
    ScoreboardTests.cpp
    TrickTests.cpp
    GoldenGameTests.cpp
    RandomPlayPropertyTests.cpp
    RoundHistoryTests.cpp
)

target_link_libraries(romanian_whist_engine_tests PRIVATE
    RomanianWhist::engine
    Catch2::Catch2WithMain
)

target_include_directories(romanian_whist_engine_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(romanian_whist_engine_tests PRIVATE cxx_std_20)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(romanian_whist_engine_tests)
