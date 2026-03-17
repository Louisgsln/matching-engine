#include "MatchingEngine.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>

MatchingEngine::MatchingEngine(OrderBook& ob)
    : orderBook(ob)
{
    orderBook.setTradeCallback([this](const Trade& t) { updateStats(t); });
}

MatchingEngine::~MatchingEngine()
{
    stop();
}

bool MatchingEngine::addInstrument(const Instrument& instr)
{
    if (instruments.count(instr.symbol)) {
        std::cerr << "  [ENGINE] Instrument " << instr.symbol << " already exists\n";
        return false;
    }
    instruments[instr.symbol] = instr;
    std::cout << "  [ENGINE] Instrument " << instr.symbol << " added\n";
    return true;
}

const Instrument* MatchingEngine::getInstrument(const std::string& symbol) const
{
    auto it = instruments.find(symbol);
    return (it != instruments.end()) ? &it->second : nullptr;
}

const std::map<std::string, Instrument>& MatchingEngine::getInstruments() const
{
    return instruments;
}

bool MatchingEngine::submitOrder(Order& order)
{
    const Instrument* instr = getInstrument(order.symbol);
    if (!instr) {
        std::cerr << "  [ENGINE] Unknown instrument: " << order.symbol << "\n";
        return false;
    }

    if (instr->state != State::ACTIVE) {
        std::cerr << "  [ENGINE] Instrument " << order.symbol << " is not ACTIVE\n";
        return false;
    }

    if (!order.validatePrice(*instr) || !order.validateQuantity(*instr)) {
        return false;
    }

    order.id = getNextOrderId();
    orderBook.addOrder(order);

    matchingAttempts.fetch_add(1, std::memory_order_relaxed);
    orderBook.matchOrders();

    return true;
}

int MatchingEngine::getNextOrderId()
{
    return nextOrderId.fetch_add(1, std::memory_order_relaxed);
}

void MatchingEngine::start()
{
    if (!running.load())
    {
        running.store(true);
        resetDailyStats();
        engineThread = std::thread(&MatchingEngine::run, this);
        std::cout << "  [ENGINE] Background matching started\n";
    }
}

void MatchingEngine::stop()
{
    if (running.load())
    {
        running.store(false);
        if (engineThread.joinable())
            engineThread.join();
        std::cout << "  [ENGINE] Background matching stopped\n";
    }
}

bool MatchingEngine::isRunning() const
{
    return running.load();
}

void MatchingEngine::run()
{
    auto lastStatus = std::chrono::system_clock::now();

    while (running.load())
    {
        matchingAttempts.fetch_add(1, std::memory_order_relaxed);
        orderBook.matchOrders();

        auto now = std::chrono::system_clock::now();
        if (now - lastStatus > std::chrono::seconds(60))
        {
            std::lock_guard<std::mutex> lock(displayMutex);
            displayStatus();
            lastStatus = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void MatchingEngine::updateStats(const Trade& trade)
{
    double value = trade.price * trade.quantity;

    dailyTradeCount.fetch_add(1, std::memory_order_relaxed);
    totalTradeCount.fetch_add(1, std::memory_order_relaxed);
    successfulMatches.fetch_add(1, std::memory_order_relaxed);

    double cur = dailyVolume.load(std::memory_order_relaxed);
    dailyVolume.store(cur + value, std::memory_order_relaxed);

    double tot = totalVolume.load(std::memory_order_relaxed);
    totalVolume.store(tot + value, std::memory_order_relaxed);
}

void MatchingEngine::resetDailyStats()
{
    dailyTradeCount.store(0);
    dailyVolume.store(0.0);
    matchingAttempts.store(0);
    successfulMatches.store(0);
}

EngineStats MatchingEngine::getStats(const std::string& symbol) const
{
    EngineStats s;
    s.dailyTrades = dailyTradeCount.load(std::memory_order_relaxed);
    s.dailyVolume = dailyVolume.load(std::memory_order_relaxed);
    s.totalTrades = totalTradeCount.load(std::memory_order_relaxed);
    s.totalVolume = totalVolume.load(std::memory_order_relaxed);
    s.matchingAttempts = matchingAttempts.load(std::memory_order_relaxed);
    s.successfulMatches = successfulMatches.load(std::memory_order_relaxed);

    auto bids = orderBook.snapshotBidLevels(symbol);
    auto asks = orderBook.snapshotAskLevels(symbol);

    if (!bids.empty()) s.bestBid = bids.front().price;
    if (!asks.empty()) s.bestAsk = asks.front().price;

    if (s.bestBid > 0.0 && s.bestAsk > 0.0) {
        s.spread = s.bestAsk - s.bestBid;
        s.mid = (s.bestAsk + s.bestBid) / 2.0;
    }

    return s;
}

void MatchingEngine::displayStatus() const
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);

    std::cout << "\n  \033[1m=== Engine Status at "
        << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
        << " ===\033[0m\n";
    std::cout << "  Running:           " << (running.load() ? "Yes" : "No") << "\n";
    std::cout << "  Instruments:       " << instruments.size() << "\n";
    std::cout << "  Daily Trades:      " << dailyTradeCount.load() << "\n";
    std::cout << "  Daily Volume:      " << std::fixed << std::setprecision(2)
        << dailyVolume.load() << "\n";
    std::cout << "  Total Trades:      " << totalTradeCount.load() << "\n";
    std::cout << "  Total Volume:      " << totalVolume.load() << "\n";
    std::cout << "  Match Attempts:    " << matchingAttempts.load() << "\n";
    std::cout << "  Successful:        " << successfulMatches.load() << "\n";
    std::cout << "  Bid Levels:        " << orderBook.bidLevelCount() << "\n";
    std::cout << "  Ask Levels:        " << orderBook.askLevelCount() << "\n";
}