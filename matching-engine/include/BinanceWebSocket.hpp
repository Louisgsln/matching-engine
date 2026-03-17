#ifndef BINANCEWEBSOCKET_HPP
#define BINANCEWEBSOCKET_HPP

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <deque>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct WsTrade
{
    long long   id;
    std::string symbol;
    double      price;
    double      qty;
    bool        isBuyerMaker;
    long long   timestamp;
};

struct WsBookLevel
{
    double price;
    double qty;
};

struct WsBookSnapshot
{
    std::vector<WsBookLevel> bids;
    std::vector<WsBookLevel> asks;
    long long lastUpdateId = 0;
};

class BinanceWebSocket
{
public:
    using BookCallback = std::function<void(const WsBookSnapshot&)>;
    using TradeCallback = std::function<void(const WsTrade&)>;

    BinanceWebSocket();
    ~BinanceWebSocket();

    void setBookCallback(BookCallback cb);
    void setTradeCallback(TradeCallback cb);

    void start(const std::string& symbol);
    void stop();
    bool isConnected() const;

    // Thread-safe access to latest data
    WsBookSnapshot getLatestBook() const;
    std::deque<WsTrade> getRecentTrades(int maxCount = 50) const;

private:
    void runDepthStream(const std::string& symbol);
    void runTradeStream(const std::string& symbol);
    void connectAndStream(const std::string& host, const std::string& path,
        std::function<void(const std::string&)> onMessage);

    BookCallback  bookCallback;
    TradeCallback tradeCallback;

    std::atomic<bool> running{ false };
    std::thread       depthThread;
    std::thread       tradeThread;

    mutable std::mutex bookMutex;
    WsBookSnapshot     latestBook;

    mutable std::mutex tradeMutex;
    std::deque<WsTrade> recentTrades;
    static constexpr int MAX_TRADES = 200;
};

#endif