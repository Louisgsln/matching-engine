#include "BinanceClient.hpp"
#include <curl/curl.h>
#include <iostream>
#include <stdexcept>
#include <sstream>

// ─── libcurl write callback ─────────────────────────────────────
static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t totalSize = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

BinanceClient::BinanceClient(const std::string& baseUrl)
    : baseUrl(baseUrl)
{
}

std::string BinanceClient::httpGet(const std::string& url)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize libcurl");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // User-Agent header
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: MatchingEngine-CPP/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("HTTP request failed: ") + curl_easy_strerror(res));
    }

    if (httpCode != 200) {
        throw std::runtime_error("HTTP " + std::to_string(httpCode) + ": " + response);
    }

    return response;
}

// ─── Connectivity ───────────────────────────────────────────────
bool BinanceClient::ping()
{
    try {
        httpGet(baseUrl + "/api/v3/ping");
        return true;
    }
    catch (...) {
        return false;
    }
}

long long BinanceClient::serverTime()
{
    auto resp = httpGet(baseUrl + "/api/v3/time");
    auto j = json::parse(resp);
    return j["serverTime"].get<long long>();
}

// ─── Order Book ─────────────────────────────────────────────────
BinanceOrderBook BinanceClient::getOrderBook(const std::string& symbol, int limit)
{
    std::string url = baseUrl + "/api/v3/depth?symbol=" + symbol
        + "&limit=" + std::to_string(limit);

    auto resp = httpGet(url);
    auto j = json::parse(resp);

    BinanceOrderBook book;
    book.lastUpdateId = j["lastUpdateId"].get<long long>();

    for (const auto& bid : j["bids"])
    {
        BinanceOrderBookEntry entry;
        entry.price = std::stod(bid[0].get<std::string>());
        entry.quantity = std::stod(bid[1].get<std::string>());
        book.bids.push_back(entry);
    }

    for (const auto& ask : j["asks"])
    {
        BinanceOrderBookEntry entry;
        entry.price = std::stod(ask[0].get<std::string>());
        entry.quantity = std::stod(ask[1].get<std::string>());
        book.asks.push_back(entry);
    }

    return book;
}

// ─── 24h Ticker ─────────────────────────────────────────────────
BinanceTicker BinanceClient::getTicker24h(const std::string& symbol)
{
    std::string url = baseUrl + "/api/v3/ticker/24hr?symbol=" + symbol;
    auto resp = httpGet(url);
    auto j = json::parse(resp);

    BinanceTicker t;
    t.symbol = j["symbol"].get<std::string>();
    t.lastPrice = std::stod(j["lastPrice"].get<std::string>());
    t.bidPrice = std::stod(j["bidPrice"].get<std::string>());
    t.askPrice = std::stod(j["askPrice"].get<std::string>());
    t.highPrice = std::stod(j["highPrice"].get<std::string>());
    t.lowPrice = std::stod(j["lowPrice"].get<std::string>());
    t.volume = std::stod(j["volume"].get<std::string>());
    t.priceChangePercent = std::stod(j["priceChangePercent"].get<std::string>());

    return t;
}

// ─── Symbol Info (from exchangeInfo) ────────────────────────────
BinanceSymbolInfo BinanceClient::getSymbolInfo(const std::string& symbol)
{
    std::string url = baseUrl + "/api/v3/exchangeInfo?symbol=" + symbol;
    auto resp = httpGet(url);
    auto j = json::parse(resp);

    BinanceSymbolInfo info;

    if (!j.contains("symbols") || j["symbols"].empty()) {
        throw std::runtime_error("Symbol not found: " + symbol);
    }

    auto& sym = j["symbols"][0];
    info.symbol = sym["symbol"].get<std::string>();
    info.baseAsset = sym["baseAsset"].get<std::string>();
    info.quoteAsset = sym["quoteAsset"].get<std::string>();
    info.status = sym["status"].get<std::string>();
    info.pricePrecision = sym.value("quotePrecision", 8);
    info.qtyPrecision = sym.value("baseAssetPrecision", 8);

    // Extract filters
    info.tickSize = 0.01;
    info.stepSize = 0.001;
    info.minQty = 0.001;
    info.minNotional = 10.0;

    if (sym.contains("filters"))
    {
        for (const auto& filter : sym["filters"])
        {
            std::string type = filter["filterType"].get<std::string>();

            if (type == "PRICE_FILTER") {
                info.tickSize = std::stod(filter["tickSize"].get<std::string>());
            }
            else if (type == "LOT_SIZE") {
                info.stepSize = std::stod(filter["stepSize"].get<std::string>());
                info.minQty = std::stod(filter["minQty"].get<std::string>());
            }
            else if (type == "NOTIONAL") {
                if (filter.contains("minNotional"))
                    info.minNotional = std::stod(filter["minNotional"].get<std::string>());
            }
            else if (type == "MIN_NOTIONAL") {
                info.minNotional = std::stod(filter["minNotional"].get<std::string>());
            }
        }
    }

    // Compute actual decimal precision from tickSize and stepSize
    auto countDecimals = [](double val) -> int {
        int dec = 0;
        double v = val;
        while (v < 1.0 - 1e-12 && dec < 12) { v *= 10.0; dec++; }
        return dec;
        };

    info.pricePrecision = countDecimals(info.tickSize);
    info.qtyPrecision = countDecimals(info.stepSize);

    return info;
}

// ─── Klines ─────────────────────────────────────────────────────
std::vector<std::vector<double>> BinanceClient::getKlines(const std::string& symbol,
    const std::string& interval,
    int limit)
{
    std::string url = baseUrl + "/api/v3/klines?symbol=" + symbol
        + "&interval=" + interval
        + "&limit=" + std::to_string(limit);

    auto resp = httpGet(url);
    auto j = json::parse(resp);

    std::vector<std::vector<double>> result;
    for (const auto& kline : j)
    {
        // [openTime, open, high, low, close, volume, closeTime, ...]
        result.push_back({
            static_cast<double>(kline[0].get<long long>()),  // openTime
            std::stod(kline[1].get<std::string>()),          // open
            std::stod(kline[2].get<std::string>()),          // high
            std::stod(kline[3].get<std::string>()),          // low
            std::stod(kline[4].get<std::string>()),          // close
            std::stod(kline[5].get<std::string>())           // volume
            });
    }

    return result;
}

// ─── Recent Trades ──────────────────────────────────────────────
json BinanceClient::getRecentTrades(const std::string& symbol, int limit)
{
    std::string url = baseUrl + "/api/v3/trades?symbol=" + symbol
        + "&limit=" + std::to_string(limit);

    auto resp = httpGet(url);
    return json::parse(resp);
}