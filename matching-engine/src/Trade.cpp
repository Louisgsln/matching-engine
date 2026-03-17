#include "Trade.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>

void Trade::display() const
{
    auto t = std::chrono::system_clock::to_time_t(timestamp);
    std::cout << "  Trade #" << id
        << " | " << symbol
        << " | Buy#" << buyOrderId << " x Sell#" << sellOrderId
        << " | Price: " << std::fixed << std::setprecision(8) << price
        << " | Qty: " << quantity
        << " | Value: " << std::setprecision(2) << (price * quantity)
        << " | " << std::put_time(std::localtime(&t), "%H:%M:%S")
        << "\n";
}