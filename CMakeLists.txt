cmake_minimum_required(VERSION 3.20)
project(OmniBusCazino VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 prefix path
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64" CACHE PATH "Qt6 prefix")

# OpenSSL — manual imported targets (non-standard lib layout)
set(OPENSSL_BASE "C:/Program Files/OpenSSL-Win64")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network WebSockets)
find_package(Qt6 QUIET COMPONENTS Multimedia)
if(Qt6Multimedia_FOUND)
    message(STATUS "Qt6::Multimedia found — sound effects enabled")
    add_definitions(-DHAS_QT_MULTIMEDIA)
else()
    message(STATUS "Qt6::Multimedia NOT found — using Windows native audio (winmm)")
endif()

# OMPEval — Professional poker hand evaluator
set(OMPEVAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/OMPEval")
# Only HandEvaluator for now (EquityCalculator needs std::function fix)
set(OMPEVAL_SOURCES
    ${OMPEVAL_DIR}/omp/HandEvaluator.cpp
)
# Create imported OpenSSL targets manually (lib layout: lib/VC/x64/MD/)
if(NOT TARGET OpenSSL::Crypto)
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_BASE}/lib/VC/x64/MD/libcrypto.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_BASE}/include"
    )
endif()
if(NOT TARGET OpenSSL::SSL)
    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_BASE}/lib/VC/x64/MD/libssl.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_BASE}/include"
    )
    target_link_libraries(OpenSSL::SSL INTERFACE OpenSSL::Crypto)
endif()

# nlohmann/json via FetchContent
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# Source files
set(SOURCES
    src/main.cpp
    # Models (header-only)
    src/models/Bet.h
    src/models/Player.h
    src/models/GameResult.h
    # Engine
    src/engine/ProvablyFair.h
    src/engine/ProvablyFair.cpp
    src/engine/BettingEngine.h
    src/engine/BettingEngine.cpp
    src/engine/AccountSystem.h
    src/engine/AccountSystem.cpp
    src/engine/PokerEvaluator.h
    src/engine/PokerEvaluator.cpp
    ${OMPEVAL_SOURCES}
    # UI
    src/ui/GameWidget.h
    src/ui/BetPanel.h
    src/ui/BetPanel.cpp
    src/ui/LoginWidget.h
    src/ui/LoginWidget.cpp
    # Utils
    src/utils/CardDeck.h
    src/utils/CardDeck.cpp
    src/utils/SoundEngine.h
    src/utils/SoundEngine.cpp
    # Games
    src/games/DiceGame.h
    src/games/DiceGame.cpp
    src/games/CrashGame.h
    src/games/CrashGame.cpp
    src/games/CoinFlipGame.h
    src/games/CoinFlipGame.cpp
    src/games/BlackjackGame.h
    src/games/BlackjackGame.cpp
    src/games/PokerGame.h
    src/games/PokerGame.cpp
    src/games/PokerRoom.h
    src/games/PokerRoom.cpp
    src/games/SlotsGame.h
    src/games/SlotsGame.cpp
    src/games/SportsBettingGame.h
    src/games/SportsBettingGame.cpp
    src/games/BinaryOptionsGame.h
    src/games/BinaryOptionsGame.cpp
    src/games/RaceBettingGame.h
    src/games/RaceBettingGame.cpp
    src/games/AuctionGame.h
    src/games/AuctionGame.cpp
    # Server (multiplayer poker)
    src/server/PokerProtocol.h
    src/server/PokerProtocol.cpp
    src/server/PokerTable.h
    src/server/PokerTable.cpp
    src/server/PokerServer.h
    src/server/PokerServer.cpp
    src/server/Tournament.h
    src/server/Tournament.cpp
    # Network (poker client)
    src/network/PokerClient.h
    src/network/PokerClient.cpp
    # Lobby (multiplayer poker UI)
    src/ui/LobbyWidget.h
    src/ui/LobbyWidget.cpp
    # P2P Decentralized Poker
    src/p2p/GameCode.h
    src/p2p/GameCode.cpp
    src/p2p/MentalPoker.h
    src/p2p/MentalPoker.cpp
    src/p2p/P2PDiscovery.h
    src/p2p/P2PDiscovery.cpp
    src/p2p/ReputationSystem.h
    src/p2p/ReputationSystem.cpp
    src/ui/P2PLobbyWidget.h
    src/ui/P2PLobbyWidget.cpp
    src/ui/TournamentLobbyWidget.h
    src/ui/TournamentLobbyWidget.cpp
    # App
    src/app/MainWindow.h
    src/app/MainWindow.cpp
)

set(RESOURCES
    resources/resources.qrc
)

add_executable(${PROJECT_NAME} ${SOURCES} ${RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OMPEVAL_DIR}
    "${OPENSSL_BASE}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

if(Qt6Multimedia_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Multimedia)
else()
    # Windows native audio via winmm
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
    endif()
endif()

# Copy config.json to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.json
               ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)

# Copy card assets to build directory
file(GLOB CARD_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets/cards/*.png")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets/cards")
foreach(CARD ${CARD_ASSETS})
    get_filename_component(CARD_NAME ${CARD} NAME)
    configure_file(${CARD} "${CMAKE_CURRENT_BINARY_DIR}/assets/cards/${CARD_NAME}" COPYONLY)
endforeach()
