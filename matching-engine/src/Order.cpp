#include "Order.hpp"
#include "Instrument.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

Order::Order()
    : id(0), side(OrderSide::BUY), type(OrderType::LIMIT),
    tif(TimeInForce::GTC), price(0.0), quantity(0.0), originalQty(0.0),
    timestamp(std::chrono::system_clock::now())
{
}

Order::Order(int id, const std::string& symbol, OrderSide side, OrderType type,
    TimeInForce tif, double price, double quantity)
    : id(id), symbol(symbol), side(side), type(type), tif(tif),
    price(price), quantity(quantity), originalQty(quantity),
    timestamp(std::chrono::system_clock::now())
{
}

void Order::display() const
{
    auto t = std::chrono::system_clock::to_time_t(timestamp);
    std::cout << "  Order #" << id
        << " | " << symbol
        << " | " << (side == OrderSide::BUY ? "BUY " : "SELL")
        << " | " << (type == OrderType::LIMIT ? "LIMIT " : "MARKET")
        << " | Price: " << std::fixed << std::setprecision(8) << price
        << " | Qty: " << quantity << "/" << originalQty
        << " | " << std::put_time(std::localtime(&t), "%H:%M:%S")
        << "\n";
}

bool Order::validatePrice(const Instrument& instr) const
{
    if (type == OrderType::MARKET) return true;

    if (price <= 0.0) {
        std::cerr << "  [VALIDATION] Price must be positive\n";
        return false;
    }

    // Check tick size conformity
    double remainder = std::fmod(price, instr.tickSize);
    if (std::fabs(remainder) > 1e-10 && std::fabs(remainder - instr.tickSize) > 1e-10) {
        std::cerr << "  [VALIDATION] Price " << price
            << " is not a multiple of tick size " << instr.tickSize << "\n";
        return false;
    }

    return true;
}

bool Order::validateQuantity(const Instrument& /*instr*/) const
{
    if (quantity <= 0.0) {
        std::cerr << "  [VALIDATION] Quantity must be positive\n";
        return false;
    }
    return true;
}