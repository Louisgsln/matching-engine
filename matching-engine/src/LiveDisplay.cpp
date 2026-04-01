#include "LiveDisplay.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Fonction utilitaire pour aligner le texte malgré les codes couleurs ANSI
std::string padStr(const std::string& str, int targetWidth) {
    int visibleLen = 0;
    bool inAnsi = false;
    for (char c : str) {
        if (c == '\033') inAnsi = true;
        else if (!inAnsi) visibleLen++;
        if (c == 'm' && inAnsi) inAnsi = false;
    }
    if (visibleLen < targetWidth)
        return str + std::string(targetWidth - visibleLen, ' ');
    return str;
}

LiveDisplay::LiveDisplay(BinanceWebSocket& ws, MatchingEngine& engine, OrderBook& book)
    : ws(ws), engine(engine), book(book), bookDepth(10)
{
}

LiveDisplay::~LiveDisplay()
{
    stop();
}

void LiveDisplay::clearScreen()
{
#ifdef _WIN32
    // Remplacement robuste pour Windows (évite le scrolling infini)
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { 0, 0 };
    SetConsoleCursorPosition(hConsole, pos);
#else
    std::cout << "\033[H";
#endif
}

void LiveDisplay::start(const std::string& sym, int depth)
{
    if (active.load()) return;

    symbol = sym;
    bookDepth = depth;
    active.store(true);

    initialLocalTrades = engine.getStats(sym).totalTrades;
    startTime = std::chrono::system_clock::now();
    prevRenderTrades = initialLocalTrades;
    prevRenderTime = startTime;
    smoothedTps = 0.0;

    ws.setTradeCallback([](const WsTrade&) { /* On ignore les trades Binance ici */ });

#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif

    // --- CORRECTION : Nettoyage intégral de l'écran avant de commencer ---
    std::cout << "\033[2J\033[H" << std::flush;

    displayThread = std::thread(&LiveDisplay::renderLoop, this);
}

void LiveDisplay::stop()
{
    if (!active.load()) return;
    active.store(false);
    if (displayThread.joinable()) displayThread.join();
}

bool LiveDisplay::isActive() const
{
    return active.load();
}

void LiveDisplay::renderLoop()
{
    while (active.load())
    {
        render();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void LiveDisplay::render()
{
    WsBookSnapshot wsBook = ws.getLatestBook();
    EngineStats localStats = engine.getStats(symbol);
    std::vector<Trade> localTrades = book.getTradeHistory();

    auto now = std::chrono::system_clock::now();
    long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    long long sessionLocalTrades = localStats.totalTrades - initialLocalTrades;

    // Instantaneous TPS: trades since last render / time since last render, smoothed with EMA
    double dtSec = std::chrono::duration<double>(now - prevRenderTime).count();
    long long deltaTrades = localStats.totalTrades - prevRenderTrades;
    if (dtSec > 0.0) {
        double instantTps = (double)deltaTrades / dtSec;
        smoothedTps = 0.3 * instantTps + 0.7 * smoothedTps;
    }
    prevRenderTrades = localStats.totalTrades;
    prevRenderTime = now;
    double localTps = smoothedTps;

    std::ostringstream out;
    clearScreen();

    // Ligne d'en-tête globale
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf;
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif

    out << "\n"
        << "\033[1;36m--------------------------------------------------------------------------------\n"
        << "  LIVE TRADING TERMINAL - " << std::left << std::setw(30) << symbol
        << std::put_time(&tmBuf, "%H:%M:%S") << "\n"
        << "--------------------------------------------------------------------------------\033[0m\n\n";

    std::vector<std::string> leftPane;
    std::vector<std::string> rightPane;

    // --- COLONNE GAUCHE : BINANCE BOOK ---
    double bestBid = !wsBook.bids.empty() ? wsBook.bids.front().price : 0.0;
    double bestAsk = !wsBook.asks.empty() ? wsBook.asks.front().price : 0.0;
    double spread = (bestBid > 0 && bestAsk > 0) ? bestAsk - bestBid : 0.0;

    leftPane.push_back("\033[1;33m[ MARKET DATA - BINANCE ]\033[0m");
    leftPane.push_back("\033[90m---------------------------------------\033[0m");

    // Asks
    int askCount = std::min((int)wsBook.asks.size(), bookDepth);
    for (int i = askCount - 1; i >= 0; --i) {
        std::ostringstream line;
        line << "\033[31m  " << std::fixed << std::setprecision(2) << std::setw(10) << wsBook.asks[i].price
            << "  |  " << std::setprecision(5) << std::setw(10) << wsBook.asks[i].qty << "\033[0m";
        leftPane.push_back(line.str());
    }

    // Spread
    std::ostringstream spreadLine;
    spreadLine << "\033[1;36m  >>> SPREAD: " << std::fixed << std::setprecision(2) << spread << " <<<\033[0m";
    leftPane.push_back(spreadLine.str());

    // Bids
    int bidCount = std::min((int)wsBook.bids.size(), bookDepth);
    for (int i = 0; i < bidCount; ++i) {
        std::ostringstream line;
        line << "\033[32m  " << std::fixed << std::setprecision(2) << std::setw(10) << wsBook.bids[i].price
            << "  |  " << std::setprecision(5) << std::setw(10) << wsBook.bids[i].qty << "\033[0m";
        leftPane.push_back(line.str());
    }


    // --- COLONNE DROITE : LOCAL ENGINE ---
    rightPane.push_back("\033[1;35m[ LOCAL MATCHING ENGINE ]\033[0m");

    std::ostringstream statsLine;
    statsLine << "  TPS: \033[1m" << std::fixed << std::setprecision(1) << localTps
        << "\033[0m  |  Execs: \033[1m" << sessionLocalTrades << "\033[0m";
    rightPane.push_back(statsLine.str());
    rightPane.push_back("\033[90m---------------------------------------\033[0m");

    int tradeLines = bookDepth * 2 + 1; // On aligne la hauteur sur l'Orderbook
    int startIdx = std::max(0, (int)localTrades.size() - tradeLines);

    if (localTrades.empty()) {
        rightPane.push_back("\033[90m  (No local trades yet)\033[0m");
        rightPane.push_back("\033[90m  (Type 'order' in CLI)\033[0m");
    }
    else {
        for (int i = startIdx; i < (int)localTrades.size(); ++i) {
            const Trade& tLocal = localTrades[i];
            std::ostringstream line;
            line << "  \033[32m#" << tLocal.id << "\033[0m @ "
                << std::fixed << std::setprecision(2) << tLocal.price
                << " (\033[36m" << std::setprecision(4) << tLocal.quantity << "\033[0m)";
            rightPane.push_back(line.str());
        }
    }


    // --- FUSION DES COLONNES ---
    size_t maxLines = std::max(leftPane.size(), rightPane.size());
    for (size_t i = 0; i < maxLines; ++i) {
        std::string leftStr = (i < leftPane.size()) ? leftPane[i] : "";
        std::string rightStr = (i < rightPane.size()) ? rightPane[i] : "";

        out << padStr(leftStr, 40) << " | " << rightStr << "\n";
    }

    // Pied de page
    out << "\n\033[90m--------------------------------------------------------------------------------\n";
    out << "  Press Enter to stop  |  Uptime: " << elapsed << "s\033[0m\n";

    // Ajout de lignes vides pour s'assurer que le buffer console efface le surplus potentiel du bas
    for (int i = 0; i < 5; ++i) out << "                                                                                \n";

    std::cout << out.str() << std::flush;
}