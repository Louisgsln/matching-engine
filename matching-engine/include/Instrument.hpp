#ifndef INSTRUMENT_HPP
#define INSTRUMENT_HPP

#include <string>

enum class State { ACTIVE, INACTIVE, SUSPENDED, DELISTED };

struct Instrument
{
    int         id;
    std::string symbol;          // e.g. "BTCUSDT"
    std::string baseCurrency;    // e.g. "BTC"
    std::string quoteCurrency;   // e.g. "USDT"
    State       state;
    double      refPrice;
    int         lotSize;         // min qty (in base units * 10^qtyDecimal)
    int         priceDecimal;    // number of decimals for price
    int         qtyDecimal;      // number of decimals for quantity
    double      tickSize;        // minimum price movement

    Instrument() = default;
    Instrument(int id, const std::string& symbol,
        const std::string& base, const std::string& quote,
        State state, double refPrice, int lotSize,
        int priceDecimal, int qtyDecimal, double tickSize);

    void display() const;
};

#endif