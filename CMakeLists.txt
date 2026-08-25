cmake_minimum_required(VERSION 3.21)

project(RomanianWhistEngine
    VERSION 1.0.1
    DESCRIPTION "Reusable Romanian Whist game engine"
    LANGUAGES CXX)

add_library(romanian_whist_engine STATIC
    src/Card.cpp
    src/CardValidator.cpp
    src/Deck.cpp
    src/Player.cpp
    src/PlayerList.cpp
    src/Trick.cpp
    src/Round.cpp
    src/Scoreboard.cpp
    src/GameEngine.cpp
    src/AiMoveProvider.cpp
    src/strategies/FirstCardStrategy.cpp
    src/strategies/RandomCardStrategy.cpp
)

# Consumers link against this alias, so the name is identical whether the engine
# arrives via add_subdirectory() or a future find_package().
add_library(RomanianWhist::engine ALIAS romanian_whist_engine)

target_compile_features(romanian_whist_engine PUBLIC cxx_std_20)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/romanian_whist/Version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/romanian_whist/Version.h
    @ONLY)

target_include_directories(romanian_whist_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
