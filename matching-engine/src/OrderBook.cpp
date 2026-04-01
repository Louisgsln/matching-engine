#include "OrderBook.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

OrderBook::OrderBook() : nextTradeId(1) {}

void OrderBook::setTradeCallback(TradeCallback cb)
{
    tradeCallback = std::move(cb);
}

void OrderBook::addOrder(const Order& order)
{
    std::lock_guard<std::mutex> lock(bookMutex);

    if (order.side == OrderSide::BUY)
        bidOrders[order.price].push_back(order);
    else
        askOrders[order.price].push_back(order);
}

int OrderBook::matchOrders()
{
    int tradesExecuted = 0;
    std::lock_guard<std::mutex> lock(bookMutex);

    while (!bidOrders.empty() && !askOrders.empty())
    {
        auto bestBidIt = bidOrders.begin();
        auto bestAskIt = askOrders.begin();

        // No crossing => no match possible
        if (bestBidIt->first < bestAskIt->first)
            break;

        bool matchFound = false;

        for (auto& bidOrder : bestBidIt->second)
        {
            for (auto& askOrder : bestAskIt->second)
            {
                // Must be same symbol
                if (bidOrder.symbol != askOrder.symbol)
                    continue;

                double tradeQty = std::min(bidOrder.quantity, askOrder.quantity);

                Trade trade;
                trade.id = nextTradeId++;
                trade.buyOrderId = bidOrder.id;
                trade.sellOrderId = askOrder.id;
                trade.symbol = bidOrder.symbol;
                trade.price = askOrder.price;  // passive order determines price
                trade.quantity = tradeQty;
                trade.timestamp = std::chrono::system_clock::now();

                trades.push_back(trade);
                if (trades.size() > MAX_TRADE_HISTORY)
                    trades.erase(trades.begin());

                // Notify engine
                if (tradeCallback) tradeCallback(trade);

                bidOrder.quantity -= tradeQty;
                askOrder.quantity -= tradeQty;

                tradesExecuted++;
                matchFound = true;

                /*std::cout << "\n  \033[32m[TRADE]\033[0m #" << trade.id
                    << " " << trade.symbol
                    << " Qty=" << std::fixed << std::setprecision(6) << tradeQty
                    << " @ " << std::setprecision(8) << trade.price
                    << " (Buy#" << trade.buyOrderId
                    << " x Sell#" << trade.sellOrderId << ")\n"; */

                if (bidOrder.quantity <= 1e-12 || askOrder.quantity <= 1e-12)
                    break;
            }
            if (matchFound) break;
        }

        cleanupExecutedOrders();
        if (!matchFound) break;
    }

    return tradesExecuted;
}

void OrderBook::cleanupExecutedOrders()
{
    auto cleanup = [](auto& orderMap) {
        for (auto it = orderMap.begin(); it != orderMap.end();)
        {
            it->second.erase(
                std::remove_if(it->second.begin(), it->second.end(),
                    [](const Order& o) { return o.quantity <= 1e-12; }),
                it->second.end()
            );
            if (it->second.empty()) it = orderMap.erase(it);
            else ++it;
        }
        };

    cleanup(bidOrders);
    cleanup(askOrders);
}

std::vector<PriceLevel> OrderBook::snapshotBidLevels(const std::string& symbol) const
{
    std::lock_guard<std::mutex> lock(bookMutex);
    std::vector<PriceLevel> out;

    for (const auto& [px, orders] : bidOrders)
    {
        double q = 0.0;
        for (const auto& o : orders)
            if (o.symbol == symbol) q += o.quantity;
        if (q > 1e-12) out.push_back({ px, q });
    }
    return out;
}

std::vector<PriceLevel> OrderBook::snapshotAskLevels(const std::string& symbol) const
{
    std::lock_guard<std::mutex> lock(bookMutex);
    std::vector<PriceLevel> out;

    for (const auto& [px, orders] : askOrders)
    {
        double q = 0.0;
        for (const auto& o : orders)
            if (o.symbol == symbol) q += o.quantity;
        if (q > 1e-12) out.push_back({ px, q });
    }
    return out;
}

size_t OrderBook::bidLevelCount() const
{
    std::lock_guard<std::mutex> lock(bookMutex);
    return bidOrders.size();
}

size_t OrderBook::askLevelCount() const
{
    std::lock_guard<std::mutex> lock(bookMutex);
    return askOrders.size();
}

const Trade* OrderBook::getLastTrade() const
{
    std::lock_guard<std::mutex> lock(bookMutex);
    return trades.empty() ? nullptr : &trades.back();
}

std::vector<Trade> OrderBook::getTradeHistory() const
{
    std::lock_guard<std::mutex> lock(bookMutex);
    return trades;
}

void OrderBook::displayBook(const std::string& symbol, int depth) const
{
    auto bids = snapshotBidLevels(symbol);
    auto asks = snapshotAskLevels(symbol);

    std::cout << "\n  \033[1m=== Order Book: " << symbol << " ===\033[0m\n\n";

    // ASK side (reversed to show highest ask on top)
    std::cout << "  \033[31m--- ASKS (Sell) ---\033[0m\n";
    int askCount = std::min((int)asks.size(), depth);
    for (int i = askCount - 1; i >= 0; --i)
    {
        std::cout << "  " << std::fixed
            << std::setprecision(8) << std::setw(18) << asks[i].price
            << "  |  " << std::setprecision(6) << std::setw(14) << asks[i].qty
            << "\n";
    }

    // Spread
    if (!bids.empty() && !asks.empty())
    {
        double spread = asks.front().price - bids.front().price;
        double mid = (asks.front().price + bids.front().price) / 2.0;
        std::cout << "  \033[33m---------- Spread: " << std::setprecision(8) << spread
            << " | Mid: " << mid << " ----------\033[0m\n";
    }
    else
    {
        std::cout << "  \033[33m---------- No spread available ----------\033[0m\n";
    }

    // BID side
    std::cout << "  \033[32m--- BIDS (Buy) ---\033[0m\n";
    int bidCount = std::min((int)bids.size(), depth);
    for (int i = 0; i < bidCount; ++i)
    {
        std::cout << "  " << std::fixed
            << std::setprecision(8) << std::setw(18) << bids[i].price
            << "  |  " << std::setprecision(6) << std::setw(14) << bids[i].qty
            << "\n";
    }

    std::cout << "\n  Bid levels: " << bids.size()
        << " | Ask levels: " << asks.size() << "\n";
}