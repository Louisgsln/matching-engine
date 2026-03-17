#ifndef BINANCECLIENT_HPP
#define BINANCECLIENT_HPP

#include <string>
#include <vector>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct BinanceOrderBookEntry
{
    double price;
    double quantity;
};

struct BinanceOrderBook
{
    long long lastUpdateId;
    std::vector<BinanceOrderBookEntry> bids;
    std::vector<BinanceOrderBookEntry> asks;
};

struct BinanceTicker
{
    std::string symbol;
    double      lastPrice;
    double      bidPrice;
    double      askPrice;
    double      highPrice;
    double      lowPrice;
    double      volume;
    double      priceChangePercent;
};

struct BinanceSymbolInfo
{
    std::string symbol;
    std::string baseAsset;
    std::string quoteAsset;
    std::string status;
    int         pricePrecision;
    int         qtyPrecision;
    double      tickSize;
    double      stepSize;
    double      minQty;
    double      minNotional;
};

class BinanceClient
{
public:
    BinanceClient(const std::string& baseUrl = "https://api.binance.com");

    // Connectivity
    bool ping();
    long long serverTime();

    // Market data
    BinanceOrderBook   getOrderBook(const std::string& symbol, int limit = 20);
    BinanceTicker      getTicker24h(const std::string& symbol);
    BinanceSymbolInfo  getSymbolInfo(const std::string& symbol);
    std::vector<std::vector<double>> getKlines(const std::string& symbol,
        const std::string& interval = "1h",
        int limit = 24);
    json getRecentTrades(const std::string& symbol, int limit = 10);

private:
    std::string httpGet(const std::string& url);
    std::string baseUrl;
};

#endif