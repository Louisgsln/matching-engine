#include <iostream>
#include "OrderBook.hpp"
#include "MatchingEngine.hpp"
#include "BinanceClient.hpp"
#include "ConsoleUI.hpp"

int main()
{
    OrderBook      orderBook;
    MatchingEngine engine(orderBook);
    BinanceClient  binance("https://api.binance.com");

    ConsoleUI ui(engine, orderBook, binance);
    ui.run();

    return 0;
}