#ifndef LIVEDISPLAY_HPP
#define LIVEDISPLAY_HPP

#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <chrono>
#include <vector>
#include "BinanceWebSocket.hpp"
#include "MatchingEngine.hpp"
#include "OrderBook.hpp"

class LiveDisplay
{
public:
    explicit LiveDisplay(BinanceWebSocket& ws, MatchingEngine& engine, OrderBook& book);
    ~LiveDisplay();

    void start(const std::string& symbol, int bookDepth = 15);
    void stop();
    bool isActive() const;

private:
    void renderLoop();
    void render();
    void clearScreen();

    BinanceWebSocket& ws;
    MatchingEngine& engine;
    OrderBook& book;

    std::string       symbol;
    int               bookDepth;

    std::atomic<bool> active{ false };
    std::thread       displayThread;

    long long initialLocalTrades{ 0 };
    std::chrono::system_clock::time_point startTime;
};

#endif