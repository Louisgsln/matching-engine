#include "ConsoleUI.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <random>

ConsoleUI::ConsoleUI(MatchingEngine& engine, OrderBook& book, BinanceClient& client)
    : engine(engine), book(book), client(client), currentSymbol("BTCUSDT"),
    wsClient(), liveDisplay(wsClient, engine, book)
{
}
// -----------------------------------

void ConsoleUI::printHeader(const std::string& title)
{
    std::cout << "\n  \033[1;36m╔══════════════════════════════════════════════╗\033[0m\n";
    std::cout << "  \033[1;36m║\033[0m  " << std::left << std::setw(43) << title << "\033[1;36m║\033[0m\n";
    std::cout << "  \033[1;36m╚════════════════════════════════════════════════╝\033[0m\n";
}

void ConsoleUI::printSeparator()
{
    std::cout << "  ──────────────────────────────────────────────\n";
}

std::string ConsoleUI::readLine(const std::string& prompt)
{
    std::cout << "  " << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

double ConsoleUI::readDouble(const std::string& prompt)
{
    std::string s = readLine(prompt);
    try { return std::stod(s); }
    catch (...) { return -1.0; }
}

int ConsoleUI::readInt(const std::string& prompt)
{
    std::string s = readLine(prompt);
    try { return std::stoi(s); }
    catch (...) { return -1; }
}

void ConsoleUI::showHelp()
{
    printHeader("Commands");
    std::cout << R"(
  \033[1mLive Streaming\033[0m
    stream       - LIVE order book + trades (WebSocket)

  \033[1mMarket Data (Binance API)\033[0m
    bticker      - 24h ticker from Binance
    bbook        - Order book from Binance
    bklines      - Klines/candlesticks from Binance
    btrades      - Recent trades from Binance
    import       - Import Binance book into local engine

  \033[1mLocal Engine\033[0m
    order        - Create a new order (BUY/SELL)
    book         - Display local order book
    trades       - Show local trade history
    status       - Engine statistics

  \033[1mInstruments\033[0m
    addinstr     - Add instrument from Binance symbol info
    instruments  - List all instruments
    symbol <SYM> - Change active symbol (e.g. symbol ETHUSDT)

  \033[1mEngine\033[0m
    start        - Start background matching
    stop         - Stop background matching

  \033[1mGeneral\033[0m
    help         - Show this help
    clear        - Clear screen
    quit / exit  - Exit program
)";
    std::cout << "\n  Active symbol: \033[1;33m" << currentSymbol << "\033[0m\n";
}

void ConsoleUI::showBinanceTicker()
{
    try {
        auto t = client.getTicker24h(currentSymbol);
        printHeader("Binance 24h Ticker: " + currentSymbol);
        std::cout << std::fixed;
        std::cout << "  Last Price:    " << std::setprecision(8) << t.lastPrice << "\n";
        std::cout << "  Bid:           " << t.bidPrice << "\n";
        std::cout << "  Ask:           " << t.askPrice << "\n";
        std::cout << "  High:          " << t.highPrice << "\n";
        std::cout << "  Low:           " << t.lowPrice << "\n";
        std::cout << "  Volume (24h):  " << std::setprecision(2) << t.volume << "\n";
        std::cout << "  Change:        " << std::setprecision(2) << t.priceChangePercent << "%\n";
    }
    catch (const std::exception& e) {
        std::cerr << "  \033[31m[ERROR]\033[0m " << e.what() << "\n";
    }
}

void ConsoleUI::showBinanceBook()
{
    try {
        int depth = readInt("Depth (default 10): ");
        if (depth <= 0) depth = 10;

        auto ob = client.getOrderBook(currentSymbol, depth);
        printHeader("Binance Book: " + currentSymbol);

        std::cout << "\n  \033[31m--- ASKS ---\033[0m\n";
        for (int i = (int)ob.asks.size() - 1; i >= 0; --i) {
            std::cout << "  " << std::fixed
                << std::setprecision(8) << std::setw(18) << ob.asks[i].price
                << "  |  " << std::setprecision(6) << std::setw(14) << ob.asks[i].quantity
                << "\n";
        }

        if (!ob.bids.empty() && !ob.asks.empty()) {
            double spread = ob.asks.front().price - ob.bids.front().price;
            std::cout << "  \033[33m---------- Spread: " << std::setprecision(8) << spread
                << " ----------\033[0m\n";
        }

        std::cout << "  \033[32m--- BIDS ---\033[0m\n";
        for (const auto& b : ob.bids) {
            std::cout << "  " << std::fixed
                << std::setprecision(8) << std::setw(18) << b.price
                << "  |  " << std::setprecision(6) << std::setw(14) << b.quantity
                << "\n";
        }

        std::cout << "\n  Update ID: " << ob.lastUpdateId << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "  \033[31m[ERROR]\033[0m " << e.what() << "\n";
    }
}

void ConsoleUI::showBinanceKlines()
{
    try {
        std::string interval = readLine("Interval (1m/5m/15m/1h/4h/1d) [1h]: ");
        if (interval.empty()) interval = "1h";
        int limit = readInt("Candles (default 12): ");
        if (limit <= 0) limit = 12;

        auto klines = client.getKlines(currentSymbol, interval, limit);
        printHeader("Klines " + currentSymbol + " (" + interval + ")");

        std::cout << "  " << std::left
            << std::setw(22) << "Time"
            << std::setw(16) << "Open"
            << std::setw(16) << "High"
            << std::setw(16) << "Low"
            << std::setw(16) << "Close"
            << std::setw(14) << "Volume"
            << "\n";
        printSeparator();

        for (const auto& k : klines) {
            time_t t = static_cast<time_t>(k[0] / 1000.0);
            std::cout << "  " << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M")
                << "  " << std::fixed << std::setprecision(8)
                << std::setw(18) << k[1]
                << std::setw(20) << k[2]
                << std::setw(20) << k[3]
                << std::setw(20) << k[4]
                << std::setw(14) << k[5]
                << "\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "  \033[31m[ERROR]\033[0m " << e.what() << "\n";
    }
}

void ConsoleUI::showBinanceTrades()
{
    try {
        int limit = readInt("Number of trades (default 10): ");
        if (limit <= 0) limit = 10;

        auto trades = client.getRecentTrades(currentSymbol, limit);
        printHeader("Binance Recent Trades: " + currentSymbol);

        std::cout << "  " << std::left
            << std::setw(12) << "ID"
            << std::setw(16) << "Price"
            << std::setw(14) << "Qty"
            << std::setw(8) << "Side"
            << std::setw(12) << "Time"
            << "\n";
        printSeparator();

        for (const auto& t : trades) {
            time_t ts = static_cast<time_t>(t["time"].get<long long>() / 1000);
            bool isBuyerMaker = t["isBuyerMaker"].get<bool>();
            std::cout << "  "
                << std::setw(12) << t["id"].get<long long>()
                << std::setw(16) << t["price"].get<std::string>()
                << std::setw(14) << t["qty"].get<std::string>()
                << (isBuyerMaker ? "\033[31mSELL\033[0m    " : "\033[32mBUY \033[0m    ")
                << std::put_time(std::localtime(&ts), "%H:%M:%S")
                << "\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "  \033[31m[ERROR]\033[0m " << e.what() << "\n";
    }
}

void ConsoleUI::addInstrument()
{
    try {
        std::string sym = readLine("Symbol to fetch from Binance [" + currentSymbol + "]: ");
        if (sym.empty()) sym = currentSymbol;

        // Uppercase
        std::transform(sym.begin(), sym.end(), sym.begin(), ::toupper);

        std::cout << "  Fetching exchange info for " << sym << "...\n";
        auto info = client.getSymbolInfo(sym);

        Instrument instr;
        instr.id = (int)engine.getInstruments().size() + 1;
        instr.symbol = info.symbol;
        instr.baseCurrency = info.baseAsset;
        instr.quoteCurrency = info.quoteAsset;
        instr.state = (info.status == "TRADING") ? State::ACTIVE : State::SUSPENDED;
        instr.refPrice = 0.0;
        instr.lotSize = 1;
        instr.priceDecimal = info.pricePrecision;
        instr.qtyDecimal = info.qtyPrecision;
        instr.tickSize = info.tickSize;

        // Fetch current price as refPrice
        try {
            auto ticker = client.getTicker24h(sym);
            instr.refPrice = ticker.lastPrice;
        }
        catch (...) {}

        if (engine.addInstrument(instr)) {
            std::cout << "  \033[32m[OK]\033[0m Instrument added:\n";
            instr.display();
            currentSymbol = sym;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "  \033[31m[ERROR]\033[0m " << e.what() << "\n";
    }
}

void ConsoleUI::listInstruments()
{
    const auto& instrs = engine.getInstruments();
    printHeader("Instruments (" + std::to_string(instrs.size()) + ")");
    if (instrs.empty()) {
        std::cout << "  No instruments. Use 'addinstr' to add from Binance.\n";
        return;
    }
    for (const auto& [sym, instr] : instrs) {
        instr.display();
    }
}

void ConsoleUI::createOrder()
{
    const Instrument* instr = engine.getInstrument(currentSymbol);
    if (!instr) {
        std::cerr << "  \033[31m[ERROR]\033[0m No instrument loaded for " << currentSymbol
            << ". Use 'addinstr' first.\n";
        return;
    }

    printHeader("New Order: " + currentSymbol);

    std::string sideStr = readLine("Side (buy/sell): ");
    std::transform(sideStr.begin(), sideStr.end(), sideStr.begin(), ::toupper);
    OrderSide side = (sideStr == "SELL" || sideStr == "S") ? OrderSide::SELL : OrderSide::BUY;

    std::string typeStr = readLine("Type (limit/market) [limit]: ");
    std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);
    OrderType type = (typeStr == "MARKET" || typeStr == "M") ? OrderType::MARKET : OrderType::LIMIT;

    double price = 0.0;
    if (type == OrderType::LIMIT) {
        price = readDouble("Price: ");
        if (price <= 0) { std::cerr << "  Invalid price\n"; return; }
    }

    double qty = readDouble("Quantity: ");
    if (qty <= 0) { std::cerr << "  Invalid quantity\n"; return; }

    Order order(0, currentSymbol, side, type, TimeInForce::GTC, price, qty);

    if (engine.submitOrder(order)) {
        std::cout << "  \033[32m[OK]\033[0m Order submitted:\n";
        order.display();
    }
    else {
        std::cerr << "  \033[31m[REJECTED]\033[0m Order failed validation\n";
    }
}

void ConsoleUI::importBinanceBook()
{
    const Instrument* instr = engine.getInstrument(currentSymbol);
    if (!instr) {
        std::cerr << "  \033[31m[ERROR]\033[0m No instrument for " << currentSymbol
            << ". Use 'addinstr' first.\n";
        return;
    }

    try {
        int depth = readInt("Levels to import (default 5): ");
        if (depth <= 0) depth = 5;

        std::cout << "  Fetching Binance book for " << currentSymbol << "...\n";
        auto ob = client.getOrderBook(currentSymbol, depth);

        int imported = 0;

        for (const auto& bid : ob.bids) {
            Order order(0, currentSymbol, OrderSide::BUY, OrderType::LIMIT,
                TimeInForce::GTC, bid.price, bid.quantity);
            if (engine.submitOrder(order)) imported++;
        }

        for (const auto& ask : ob.asks) {
            Order order(0, currentSymbol, OrderSide::SELL, OrderType::LIMIT,
                TimeInForce::GTC, ask.price, ask.quantity);
            if (engine.submitOrder(order)) imported++;
        }

        std::cout << "  \033[32m[OK]\033[0m Imported " << imported << " orders from Binance\n";
    }
    catch (const std::exception& e) {
        std::cerr << "  \033[31m[ERROR]\033[0m " << e.what() << "\n";
    }
}

void ConsoleUI::showOrderBook()
{
    int depth = readInt("Depth (default 10): ");
    if (depth <= 0) depth = 10;
    book.displayBook(currentSymbol, depth);
}

void ConsoleUI::showStatus()
{
    auto stats = engine.getStats(currentSymbol);
    printHeader("Engine Stats - " + currentSymbol);

    std::cout << std::fixed;
    std::cout << "  Running:          " << (engine.isRunning() ? "Yes" : "No") << "\n";
    std::cout << "  Daily Trades:     " << stats.dailyTrades << "\n";
    std::cout << "  Daily Volume:     " << std::setprecision(2) << stats.dailyVolume << "\n";
    std::cout << "  Total Trades:     " << stats.totalTrades << "\n";
    std::cout << "  Total Volume:     " << stats.totalVolume << "\n";
    std::cout << "  Match Attempts:   " << stats.matchingAttempts << "\n";
    std::cout << "  Successful:       " << stats.successfulMatches << "\n";
    printSeparator();
    std::cout << "  Best Bid:         " << std::setprecision(8) << stats.bestBid << "\n";
    std::cout << "  Best Ask:         " << stats.bestAsk << "\n";
    std::cout << "  Spread:           " << stats.spread << "\n";
    std::cout << "  Mid:              " << stats.mid << "\n";
}

void ConsoleUI::showTradeHistory()
{
    auto trades = book.getTradeHistory();
    printHeader("Trade History (" + std::to_string(trades.size()) + " trades)");
    if (trades.empty()) {
        std::cout << "  No trades yet.\n";
        return;
    }

    // Show last 20
    int start = std::max(0, (int)trades.size() - 20);
    for (int i = start; i < (int)trades.size(); ++i) {
        trades[i].display();
    }
}

void ConsoleUI::run()
{
    std::cout << R"(
  ╔═══════════════════════════════════════════════════════╗
  ║                                                       ║
  ║   ███╗   ███╗ █████╗ ████████╗ ██████╗██╗  ██╗        ║
  ║   ████╗ ████║██╔══██╗╚══██╔══╝██╔════╝██║  ██║        ║
  ║   ██╔████╔██║███████║   ██║   ██║     ███████║        ║
  ║   ██║╚██╔╝██║██╔══██║   ██║   ██║     ██╔══██║        ║
  ║   ██║ ╚═╝ ██║██║  ██║   ██║   ╚██████╗██║  ██║        ║
  ║   ╚═╝     ╚═╝╚═╝  ╚═╝   ╚═╝    ╚═════╝╚═╝  ╚═╝        ║
  ║                                                       ║
  ║      C++ Matching Engine + Binance API                ║
  ║      Type 'help' for commands                         ║
  ║                                                       ║
  ╚═══════════════════════════════════════════════════════╝
)";

    // Test Binance connectivity
    std::cout << "  Testing Binance API connection... ";
    if (client.ping()) {
        std::cout << "\033[32mOK\033[0m\n";
        auto serverMs = client.serverTime();
        time_t t = serverMs / 1000;
        std::cout << "  Server time: " << std::put_time(std::gmtime(&t), "%Y-%m-%d %H:%M:%S UTC") << "\n";
    }
    else {
        std::cout << "\033[31mFAILED\033[0m (some features will be unavailable)\n";
    }

    std::cout << "\n  Active symbol: \033[1;33m" << currentSymbol << "\033[0m\n";
    std::cout << "  Tip: Use 'addinstr' to load an instrument from Binance\n\n";

    while (true)
    {
        std::string input = readLine("\033[1;35m❯\033[0m ");
        if (input.empty()) continue;

        // Parse command and args
        std::istringstream iss(input);
        std::string cmd;
        iss >> cmd;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "quit" || cmd == "exit" || cmd == "q") {
            engine.stop();
            std::cout << "  Goodbye!\n";
            break;
        }
        else if (cmd == "help" || cmd == "h" || cmd == "?")    showHelp();
        else if (cmd == "stream" || cmd == "live")               startStream();
        else if (cmd == "bticker")                              showBinanceTicker();
        else if (cmd == "bbook")                                showBinanceBook();
        else if (cmd == "bklines" || cmd == "klines")           showBinanceKlines();
        else if (cmd == "btrades")                              showBinanceTrades();
        else if (cmd == "import")                               importBinanceBook();
        else if (cmd == "order" || cmd == "o")                  createOrder();
        else if (cmd == "book" || cmd == "b")                   showOrderBook();
        else if (cmd == "trades" || cmd == "t")                 showTradeHistory();
        else if (cmd == "status" || cmd == "stats" || cmd == "s") showStatus();
        else if (cmd == "addinstr" || cmd == "add")             addInstrument();
        else if (cmd == "instruments" || cmd == "instr" || cmd == "i") listInstruments();
        else if (cmd == "start")                                engine.start();
        else if (cmd == "stop")                                 engine.stop();
        else if (cmd == "clear" || cmd == "cls")                std::cout << "\033[2J\033[H";
        else if (cmd == "symbol" || cmd == "sym") {
            std::string newSym;
            iss >> newSym;
            if (newSym.empty()) newSym = readLine("New symbol: ");
            std::transform(newSym.begin(), newSym.end(), newSym.begin(), ::toupper);
            if (!newSym.empty()) {
                currentSymbol = newSym;
                std::cout << "  Active symbol: \033[1;33m" << currentSymbol << "\033[0m\n";
            }
        }
        else {
            std::cerr << "  Unknown command: " << cmd << ". Type 'help' for commands.\n";
        }
    }
}

void ConsoleUI::startStream()
{
    std::string sym = currentSymbol;

    if (!engine.getInstrument(sym)) {
        std::cout << "  \033[31m[ERROR]\033[0m Instrument " << sym << " is not loaded.\n";
        std::cout << "  Please type 'addinstr' to load it from Binance before starting the stream.\n";
        return;
    }

    std::cout << "  Starting live stream for " << sym << "...\n";
    std::cout << "  Press Enter to stop.\n\n";

    wsClient.start(sym);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    liveDisplay.start(sym, 10);

    // --- LE NOUVEAU BOT : MARKET SIMULATOR ---
    std::atomic<bool> botRunning{ true };
    std::thread botThread([&]() {
        // On récupère les infos de l'instrument pour avoir le vrai Tick Size
        const Instrument* instr = engine.getInstrument(sym);
        double tickSize = instr ? instr->tickSize : 0.01;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> priceDist(-0.001, 0.001);
        std::uniform_real_distribution<> qtyDist(0.001, 0.5);
        std::uniform_int_distribution<> sideDist(0, 1);

        while (botRunning) {
            auto snap = wsClient.getLatestBook();
            if (snap.asks.empty() || snap.bids.empty()) continue;

            double midPrice = (snap.asks[0].price + snap.bids[0].price) / 2.0;

            OrderSide side = (sideDist(gen) == 0) ? OrderSide::BUY : OrderSide::SELL;

            // On force en LIMIT pour éviter le bug du prix à 0.0 des ordres MARKET
            OrderType type = OrderType::LIMIT;

            double qty = qtyDist(gen);

            // Calcul du prix et ARRONDI strict au Tick Size (ex: 0.01)
            double variation = priceDist(gen);
            double rawPrice = midPrice * (1.0 + variation);
            double price = std::round(rawPrice / tickSize) * tickSize;

            Order randomOrder(0, sym, side, type, TimeInForce::GTC, price, qty);
            engine.submitOrder(randomOrder);
        }
        });
    // ------------------------------------------

    std::string dummy;
    std::getline(std::cin, dummy);

    botRunning = false;
    botThread.join();

    liveDisplay.stop();
    wsClient.stop();

    std::cout << "\n  Stream stopped. Back to command mode.\n";
}