#ifndef TRADE_HPP
#define TRADE_HPP

#include <chrono>
#include <string>

struct Trade
{
    int         id;
    int         buyOrderId;
    int         sellOrderId;
    std::string symbol;
    double      price;
    double      quantity;
    std::chrono::system_clock::time_point timestamp;

    void display() const;
};

#endif