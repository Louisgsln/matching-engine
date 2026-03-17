#ifndef ORDER_HPP
#define ORDER_HPP

#include <chrono>
#include <string>

struct Instrument;

enum class TimeInForce { GTC, DAY, IOC, FOK };
enum class OrderSide { BUY, SELL };
enum class OrderType { LIMIT, MARKET };

struct Order
{
    int         id;
    std::string symbol;
    OrderSide   side;
    OrderType   type;
    TimeInForce tif;
    double      price;
    double      quantity;
    double      originalQty;
    std::chrono::system_clock::time_point timestamp;

    Order();
    Order(int id, const std::string& symbol, OrderSide side, OrderType type,
        TimeInForce tif, double price, double quantity);

    void display() const;
    bool validatePrice(const Instrument& instr) const;
    bool validateQuantity(const Instrument& instr) const;
};

#endif