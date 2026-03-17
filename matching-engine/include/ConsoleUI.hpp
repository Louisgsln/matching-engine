#ifndef CONSOLEUI_HPP
#define CONSOLEUI_HPP

#include <string>
#include <vector>
#include "MatchingEngine.hpp"
#include "BinanceClient.hpp"
#include "BinanceWebSocket.hpp"
#include "LiveDisplay.hpp"

class ConsoleUI
{
public:
    ConsoleUI(MatchingEngine& engine, OrderBook& book, BinanceClient& client);
    void run();

private:
    void showHelp();
    void showStatus();
    void showOrderBook();
    void showBinanceBook();
    void showBinanceTicker();
    void showBinanceKlines();
    void showBinanceTrades();
    void createOrder();
    void importBinanceBook();
    void addInstrument();
    void listInstruments();
    void showTradeHistory();
    void startStream();

    void printHeader(const std::string& title);
    void printSeparator();
    std::string readLine(const std::string& prompt);
    double readDouble(const std::string& prompt);
    int readInt(const std::string& prompt);

    MatchingEngine& engine;
    OrderBook& book;
    BinanceClient& client;
    std::string currentSymbol;

    BinanceWebSocket wsClient;
    LiveDisplay liveDisplay;
};

#endif