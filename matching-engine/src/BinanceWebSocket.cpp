#include "BinanceWebSocket.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <iostream>
#include <algorithm>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using     tcp = net::ip::tcp;

BinanceWebSocket::BinanceWebSocket() {}

BinanceWebSocket::~BinanceWebSocket()
{
    stop();
}

void BinanceWebSocket::setBookCallback(BookCallback cb) { bookCallback = std::move(cb); }
void BinanceWebSocket::setTradeCallback(TradeCallback cb) { tradeCallback = std::move(cb); }

void BinanceWebSocket::start(const std::string& symbol)
{
    if (running.load()) return;
    running.store(true);

    std::string sym = symbol;
    std::transform(sym.begin(), sym.end(), sym.begin(), ::tolower);

    depthThread = std::thread(&BinanceWebSocket::runDepthStream, this, sym);
    tradeThread = std::thread(&BinanceWebSocket::runTradeStream, this, sym);

    std::cout << "  [WS] Streaming started for " << symbol << "\n";
}

void BinanceWebSocket::stop()
{
    if (!running.load()) return;
    running.store(false);

    if (depthThread.joinable()) depthThread.join();
    if (tradeThread.joinable()) tradeThread.join();

    std::cout << "  [WS] Streaming stopped\n";
}

bool BinanceWebSocket::isConnected() const
{
    return running.load();
}

WsBookSnapshot BinanceWebSocket::getLatestBook() const
{
    std::lock_guard<std::mutex> lock(bookMutex);
    return latestBook;
}

std::deque<WsTrade> BinanceWebSocket::getRecentTrades(int maxCount) const
{
    std::lock_guard<std::mutex> lock(tradeMutex);
    std::deque<WsTrade> result;
    int count = std::min(maxCount, (int)recentTrades.size());
    auto it = recentTrades.end() - count;
    for (; it != recentTrades.end(); ++it)
        result.push_back(*it);
    return result;
}

// ─── Core WebSocket connection logic ────────────────────────────
void BinanceWebSocket::connectAndStream(const std::string& host,
    const std::string& path,
    std::function<void(const std::string&)> onMessage)
{
    while (running.load())
    {
        try
        {
            net::io_context ioc;
            ssl::context ctx{ ssl::context::tlsv12_client };
            ctx.set_default_verify_paths();

            tcp::resolver resolver{ ioc };
            auto const results = resolver.resolve(host, "443");

            // SSL + TCP connection (Utilisation de beast::tcp_stream au lieu de tcp::socket)
            websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws{ ioc, ctx };

            auto& tcpStream = beast::get_lowest_layer(ws);
            // beast::tcp_stream prend directement l'ensemble des résultats de résolution,
            // ce qui est plus robuste que de prendre seulement le premier (*results.begin())
            tcpStream.connect(results);

            // SNI hostname
            if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str()))
                throw beast::system_error(beast::error_code(
                    static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));

            ws.next_layer().handshake(ssl::stream_base::client);

            // WebSocket handshake
            ws.set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req) {
                    req.set(http::field::user_agent, "MatchingEngine-CPP/1.0");
                }));

            ws.handshake(host, path);

            // Set timeout
            beast::flat_buffer buffer;

            while (running.load())
            {
                buffer.clear();

                // Non-blocking: use a small timeout
                tcpStream.expires_after(std::chrono::seconds(5));

                beast::error_code ec;
                ws.read(buffer, ec);

                if (ec == websocket::error::closed) break;
                if (ec) {
                    // Timeout or error — retry
                    if (running.load()) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                    break;
                }

                std::string msg = beast::buffers_to_string(buffer.data());
                onMessage(msg);
            }

            // Clean close
            beast::error_code ec;
            ws.close(websocket::close_code::normal, ec);
        }
        catch (const std::exception& e)
        {
            if (running.load()) {
                std::cerr << "  [WS] Connection error: " << e.what() << " — reconnecting...\n";
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }
    }
}

// ─── Depth stream (partial book, top 20 levels) ────────────────
void BinanceWebSocket::runDepthStream(const std::string& symbol)
{
    // Use partial book depth stream: updates every 100ms with top 20 levels
    std::string path = "/ws/" + symbol + "@depth20@100ms";

    connectAndStream("stream.binance.com", path, [this](const std::string& msg) {
        try
        {
            auto j = json::parse(msg);

            WsBookSnapshot snap;
            snap.lastUpdateId = j.value("lastUpdateId", 0LL);

            for (const auto& bid : j["bids"])
            {
                WsBookLevel lvl;
                lvl.price = std::stod(bid[0].get<std::string>());
                lvl.qty = std::stod(bid[1].get<std::string>());
                if (lvl.qty > 0.0) snap.bids.push_back(lvl);
            }

            for (const auto& ask : j["asks"])
            {
                WsBookLevel lvl;
                lvl.price = std::stod(ask[0].get<std::string>());
                lvl.qty = std::stod(ask[1].get<std::string>());
                if (lvl.qty > 0.0) snap.asks.push_back(lvl);
            }

            {
                std::lock_guard<std::mutex> lock(bookMutex);
                latestBook = snap;
            }

            if (bookCallback) bookCallback(snap);
        }
        catch (const std::exception& e)
        {
            std::cerr << "  [WS] Parse error (depth): " << e.what() << "\n";
        }
        });
}

// ─── Trade stream (individual trades) ───────────────────────────
void BinanceWebSocket::runTradeStream(const std::string& symbol)
{
    std::string path = "/ws/" + symbol + "@trade";

    connectAndStream("stream.binance.com", path, [this](const std::string& msg) {
        try
        {
            auto j = json::parse(msg);

            WsTrade trade;
            trade.id = j["t"].get<long long>();
            trade.symbol = j["s"].get<std::string>();
            trade.price = std::stod(j["p"].get<std::string>());
            trade.qty = std::stod(j["q"].get<std::string>());
            trade.isBuyerMaker = j["m"].get<bool>();
            trade.timestamp = j["T"].get<long long>();

            {
                std::lock_guard<std::mutex> lock(tradeMutex);
                recentTrades.push_back(trade);
                while ((int)recentTrades.size() > MAX_TRADES)
                    recentTrades.pop_front();
            }

            if (tradeCallback) tradeCallback(trade);
        }
        catch (const std::exception& e)
        {
            std::cerr << "  [WS] Parse error (trade): " << e.what() << "\n";
        }
        });
}