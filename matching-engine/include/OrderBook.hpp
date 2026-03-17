#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <map>
#include <vector>
#include <mutex>
#include <functional>
#include "Order.hpp"
#include "Trade.hpp"

struct PriceLevel
{
    double    price;
    double    qty;
};

class OrderBook
{
public:
    using TradeCallback = std::function<void(const Trade&)>;

    OrderBook();
    void setTradeCallback(TradeCallback cb);

    void addOrder(const Order& order);
    int  matchOrders();

    std::vector<PriceLevel> snapshotBidLevels(const std::string& symbol) const;
    std::vector<PriceLevel> snapshotAskLevels(const std::string& symbol) const;

    size_t bidLevelCount() const;
    size_t askLevelCount() const;

    const Trade* getLastTrade() const;
    std::vector<Trade> getTradeHistory() const;

    void displayBook(const std::string& symbol, int depth = 10) const;

private:
    void cleanupExecutedOrders();

    // BID: highest price first | ASK: lowest price first
    std::map<double, std::vector<Order>, std::greater<double>> bidOrders;
    std::map<double, std::vector<Order>>                       askOrders;

    int                nextTradeId;
    std::vector<Trade> trades;
    TradeCallback      tradeCallback;
    mutable std::mutex bookMutex;
};

#endif