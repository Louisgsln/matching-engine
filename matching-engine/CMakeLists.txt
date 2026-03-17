cmake_minimum_required(VERSION 3.16)
project(MatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Dependencies ────────────────────────────────────
find_package(CURL REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# ─── Sources ─────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/Instrument.cpp
    src/Order.cpp
    src/Trade.cpp
    src/OrderBook.cpp
    src/MatchingEngine.cpp
    src/BinanceClient.cpp
    src/BinanceWebSocket.cpp
    src/LiveDisplay.cpp
    src/ConsoleUI.cpp
)

# ─── Executable ──────────────────────────────────────
add_executable(matching_engine ${SOURCES})

target_include_directories(matching_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(matching_engine PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Threads::Threads
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ─── Compiler settings ──────────────────────────────
if(MSVC)
    target_compile_options(matching_engine PRIVATE /W4)
    target_compile_definitions(matching_engine PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0A00
    )
else()
    target_compile_options(matching_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()