#include "Instrument.hpp"
#include <iostream>
#include <iomanip>

Instrument::Instrument(int id, const std::string& symbol,
    const std::string& base, const std::string& quote,
    State state, double refPrice, int lotSize,
    int priceDecimal, int qtyDecimal, double tickSize)
    : id(id), symbol(symbol), baseCurrency(base), quoteCurrency(quote),
    state(state), refPrice(refPrice), lotSize(lotSize),
    priceDecimal(priceDecimal), qtyDecimal(qtyDecimal), tickSize(tickSize)
{
}

void Instrument::display() const
{
    auto stateStr = [](State s) -> std::string {
        switch (s) {
        case State::ACTIVE:    return "ACTIVE";
        case State::INACTIVE:  return "INACTIVE";
        case State::SUSPENDED: return "SUSPENDED";
        case State::DELISTED:  return "DELISTED";
        }
        return "UNKNOWN";
        };

    std::cout << "  " << std::left << std::setw(12) << symbol
        << " | " << std::setw(5) << baseCurrency
        << "/" << std::setw(5) << quoteCurrency
        << " | State: " << std::setw(10) << stateStr(state)
        << " | Ref: " << std::fixed << std::setprecision(priceDecimal) << refPrice
        << " | Tick: " << tickSize
        << " | LotSize: " << lotSize
        << " | PriceDec: " << priceDecimal
        << " | QtyDec: " << qtyDecimal
        << "\n";
}