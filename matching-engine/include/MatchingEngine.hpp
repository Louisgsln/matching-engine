#ifndef MATCHINGENGINE_HPP
#define MATCHINGENGINE_HPP

#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <string>
#include "OrderBook.hpp"
#include "Instrument.hpp"

struct EngineStats
{
    long long dailyTrades = 0;
    double    dailyVolume = 0.0;
    long long totalTrades = 0;
    double    totalVolume = 0.0;
    long long matchingAttempts = 0;
    long long successfulMatches = 0;
    double    bestBid = 0.0;
    double    bestAsk = 0.0;
    double    spread = 0.0;
    double    mid = 0.0;
};

class MatchingEngine
{
public:
    MatchingEngine(OrderBook& ob);
    ~MatchingEngine();

    // Instrument management
    bool addInstrument(const Instrument& instr);
    const Instrument* getInstrument(const std::string& symbol) const;
    const std::map<std::string, Instrument>& getInstruments() const;

    // Order management
    bool submitOrder(Order& order);
    int  getNextOrderId();

    // Engine lifecycle
    void start();
    void stop();
    bool isRunning() const;

    // Stats
    EngineStats getStats(const std::string& symbol) const;
    void displayStatus() const;

private:
    void run();
    void updateStats(const Trade& trade);
    void resetDailyStats();

    OrderBook& orderBook;
    std::map<std::string, Instrument> instruments;

    std::atomic<int>       nextOrderId{ 1 };
    std::atomic<bool>      running{ false };
    std::thread            engineThread;

    // Stats
    std::atomic<long long> dailyTradeCount{ 0 };
    std::atomic<double>    dailyVolume{ 0.0 };
    std::atomic<long long> totalTradeCount{ 0 };
    std::atomic<double>    totalVolume{ 0.0 };
    std::atomic<long long> matchingAttempts{ 0 };
    std::atomic<long long> successfulMatches{ 0 };

    mutable std::mutex     displayMutex;
};

#endif