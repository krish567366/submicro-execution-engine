# 🔌 Broker/Exchange Integration Guide

## Overview

This guide explains how to connect the HFT system to real brokers and exchanges for live trading. The system is designed to be broker-agnostic and supports multiple integration methods.

---

## 🏦 Supported Integration Methods

### 1. FIX Protocol (Financial Information eXchange)
**Best for:** Professional trading, institutional brokers, direct exchange access

### 2. WebSocket APIs
**Best for:** Retail brokers, crypto exchanges, real-time data feeds

### 3. REST APIs
**Best for:** Order placement, account management, historical data

### 4. Direct Market Access (DMA)
**Best for:** Co-located servers, ultra-low-latency requirements

---

## 📋 Integration Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    HFT System Core                          │
│  ┌────────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │
│  │  Hawkes    │→ │   FPGA   │→ │  Quotes  │→ │   Risk   │ │
│  └────────────┘  └──────────┘  └──────────┘  └──────────┘ │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ↓
        ┌────────────────────────────────────┐
        │    Broker Adapter Layer (New)      │
        │  ┌──────────┐  ┌──────────┐       │
        │  │ Market   │  │  Order   │       │
        │  │ Data     │  │ Gateway  │       │
        │  └──────────┘  └──────────┘       │
        └────────┬───────────────┬───────────┘
                 │               │
        ┌────────▼───┐  ┌────────▼──────┐
        │  FIX/WS    │  │  FIX/REST     │
        │  Feed      │  │  Orders       │
        └────────┬───┘  └────────┬──────┘
                 │               │
        ┌────────▼───────────────▼───────┐
        │    Broker/Exchange API         │
        └────────────────────────────────┘
```

---

## 🚀 Quick Start: Crypto Exchange Integration

### Example: Binance WebSocket Integration

#### Step 1: Create WebSocket Adapter

Create `include/ws_market_data.hpp`:

```cpp
#ifndef WS_MARKET_DATA_HPP
#define WS_MARKET_DATA_HPP

#include "common_types.hpp"
#include "lockfree_queue.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

class BinanceWebSocketFeed {
public:
    BinanceWebSocketFeed(
        const std::string& symbol,
        LockFreeSPSC<MarketTick, 65536>& output_queue
    ) : symbol_(symbol), 
        output_queue_(output_queue),
        ioc_(),
        ws_(ioc_) {}

    void connect() {
        // Binance WebSocket endpoint
        const std::string host = "stream.binance.com";
        const std::string port = "9443";
        const std::string path = "/ws/" + symbol_ + "@depth@100ms";
        
        // Resolve and connect
        tcp::resolver resolver{ioc_};
        auto results = resolver.resolve(host, port);
        auto ep = net::connect(ws_.next_layer(), results);
        
        // SSL handshake
        ws_.next_layer().handshake(ssl::stream_base::client);
        
        // WebSocket handshake
        ws_.handshake(host, path);
        
        std::cout << "Connected to Binance: " << symbol_ << std::endl;
    }
    
    void start_reading() {
        read_loop();
    }
    
private:
    void read_loop() {
        ws_.async_read(buffer_, [this](beast::error_code ec, std::size_t bytes) {
            if (ec) {
                std::cerr << "WebSocket read error: " << ec.message() << std::endl;
                return;
            }
            
            // Parse JSON message
            std::string msg = beast::buffers_to_string(buffer_.data());
            buffer_.consume(buffer_.size());
            
            auto j = json::parse(msg);
            
            // Convert to MarketTick
            MarketTick tick = parse_binance_depth(j);
            
            // Push to lock-free queue
            while (!output_queue_.try_push(tick)) {
                // Busy-wait if queue full (should never happen with proper sizing)
                std::this_thread::yield();
            }
            
            // Continue reading
            read_loop();
        });
    }
    
    MarketTick parse_binance_depth(const json& j) {
        MarketTick tick;
        tick.timestamp = now();
        tick.symbol_id = 1; // Map symbol to ID
        
        // Parse bids (up to 10 levels)
        auto bids = j["bids"];
        tick.depth_levels = std::min(10, (int)bids.size());
        for (int i = 0; i < tick.depth_levels; ++i) {
            tick.bid_prices[i] = std::stod(bids[i][0].get<std::string>());
            tick.bid_sizes[i] = static_cast<uint32_t>(
                std::stod(bids[i][1].get<std::string>()) * 1000
            );
        }
        
        // Parse asks
        auto asks = j["asks"];
        for (int i = 0; i < tick.depth_levels; ++i) {
            tick.ask_prices[i] = std::stod(asks[i][0].get<std::string>());
            tick.ask_sizes[i] = static_cast<uint32_t>(
                std::stod(asks[i][1].get<std::string>()) * 1000
            );
        }
        
        return tick;
    }
    
    std::string symbol_;
    LockFreeSPSC<MarketTick, 65536>& output_queue_;
    net::io_context ioc_;
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
};

#endif // WS_MARKET_DATA_HPP
```

#### Step 2: Create Order Gateway

Create `include/rest_order_gateway.hpp`:

```cpp
#ifndef REST_ORDER_GATEWAY_HPP
#define REST_ORDER_GATEWAY_HPP

#include "common_types.hpp"
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <nlohmann/json.hpp>

class BinanceOrderGateway {
public:
    BinanceOrderGateway(
        const std::string& api_key,
        const std::string& api_secret
    ) : api_key_(api_key), api_secret_(api_secret) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~BinanceOrderGateway() {
        curl_global_cleanup();
    }
    
    bool send_order(const Order& order) {
        // Build order parameters
        std::string params = build_order_params(order);
        
        // Sign request
        std::string signature = hmac_sha256(params, api_secret_);
        params += "&signature=" + signature;
        
        // Send HTTP POST
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        std::string url = "https://api.binance.com/api/v3/order?" + params;
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("X-MBX-APIKEY: " + api_key_).c_str());
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        
        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl);
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            std::cerr << "Order send failed: " << curl_easy_strerror(res) << std::endl;
            return false;
        }
        
        // Parse response
        auto j = json::parse(response);
        if (j.contains("orderId")) {
            std::cout << "Order placed: " << j["orderId"] << std::endl;
            return true;
        }
        
        return false;
    }
    
private:
    std::string build_order_params(const Order& order) {
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        std::stringstream ss;
        ss << "symbol=" << get_symbol_string(order.symbol_id)
           << "&side=" << (order.side == Side::BUY ? "BUY" : "SELL")
           << "&type=LIMIT"
           << "&timeInForce=GTC"
           << "&quantity=" << std::fixed << std::setprecision(8) << (order.quantity / 1000.0)
           << "&price=" << std::fixed << std::setprecision(2) << order.price
           << "&timestamp=" << now_ms;
        
        return ss.str();
    }
    
    std::string hmac_sha256(const std::string& data, const std::string& key) {
        unsigned char hash[32];
        HMAC(EVP_sha256(), key.c_str(), key.length(),
             reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
             hash, nullptr);
        
        std::stringstream ss;
        for (int i = 0; i < 32; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t new_length = size * nmemb;
        s->append((char*)contents, new_length);
        return new_length;
    }
    
    std::string get_symbol_string(int symbol_id) {
        // Map internal ID to exchange symbol
        if (symbol_id == 1) return "BTCUSDT";
        if (symbol_id == 2) return "ETHUSDT";
        return "BTCUSDT";
    }
    
    std::string api_key_;
    std::string api_secret_;
};

#endif // REST_ORDER_GATEWAY_HPP
```

#### Step 3: Update CMakeLists.txt

```cmake
# Add dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Add nlohmann/json
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)

# Link libraries
target_link_libraries(hft_system
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pthread
    rt
)
```

#### Step 4: Modify main.cpp

```cpp
#include "ws_market_data.hpp"
#include "rest_order_gateway.hpp"

int main() {
    // Initialize broker connections
    BinanceWebSocketFeed market_feed("btcusdt", market_data_queue);
    market_feed.connect();
    
    BinanceOrderGateway order_gateway(
        std::getenv("BINANCE_API_KEY"),
        std::getenv("BINANCE_API_SECRET")
    );
    
    // Start market data feed in separate thread
    std::thread feed_thread([&market_feed]() {
        market_feed.start_reading();
    });
    
    // Main trading loop
    while (running) {
        MarketTick tick;
        if (market_data_queue.try_pop(tick)) {
            // [Existing HFT logic: Hawkes, FPGA, Quotes, Risk]
            
            // Send orders via broker
            if (should_send_order) {
                Order order = construct_order(quotes);
                bool success = order_gateway.send_order(order);
                if (!success) {
                    std::cerr << "Order rejected by broker!" << std::endl;
                }
            }
        }
    }
    
    feed_thread.join();
    return 0;
}
```

---

## 🏛️ Professional Broker Integration: FIX Protocol

### FIX Protocol Overview

FIX (Financial Information eXchange) is the industry standard for institutional trading.

**Advantages:**
- Industry standard (used by 99% of institutional brokers)
- Low-latency binary encoding
- Comprehensive order types
- Built-in session management
- Drop copy support

### FIX Integration with QuickFIX

#### Step 1: Install QuickFIX

```bash
# macOS
brew install quickfix

# Ubuntu
sudo apt-get install libquickfix-dev

# From source
git clone https://github.com/quickfix/quickfix.git
cd quickfix
./configure
make
sudo make install
```

#### Step 2: Create FIX Configuration

Create `config/fix_config.cfg`:

```ini
[DEFAULT]
ConnectionType=initiator
ReconnectInterval=5
FileStorePath=fix_store
FileLogPath=fix_logs
StartTime=00:00:00
EndTime=23:59:59
UseDataDictionary=Y
DataDictionary=FIX44.xml
ValidateFieldsOutOfOrder=N
ValidateFieldsHaveValues=N
ValidateUserDefinedFields=N

[SESSION]
BeginString=FIX.4.4
SenderCompID=YOUR_FIRM_ID
TargetCompID=BROKER_ID
SocketConnectHost=broker.fixgateway.com
SocketConnectPort=9876
HeartBtInt=30
ResetOnLogon=Y
```

#### Step 3: Create FIX Application

Create `include/fix_gateway.hpp`:

```cpp
#ifndef FIX_GATEWAY_HPP
#define FIX_GATEWAY_HPP

#include <quickfix/Application.h>
#include <quickfix/MessageCracker.h>
#include <quickfix/Values.h>
#include <quickfix/Mutex.h>
#include <quickfix/FileStore.h>
#include <quickfix/SocketInitiator.h>
#include <quickfix/SessionSettings.h>
#include <quickfix/FileLog.h>

#include <quickfix/fix44/NewOrderSingle.h>
#include <quickfix/fix44/ExecutionReport.h>
#include <quickfix/fix44/MarketDataRequest.h>
#include <quickfix/fix44/MarketDataSnapshotFullRefresh.h>

#include "common_types.hpp"
#include "lockfree_queue.hpp"

class FIXGateway : public FIX::Application, public FIX::MessageCracker {
public:
    FIXGateway(
        LockFreeSPSC<MarketTick, 65536>& market_queue,
        LockFreeSPSC<Order, 16384>& order_queue
    ) : market_queue_(market_queue), 
        order_queue_(order_queue),
        next_order_id_(1) {}
    
    // FIX Application interface
    void onCreate(const FIX::SessionID& sessionID) override {
        std::cout << "FIX Session created: " << sessionID << std::endl;
    }
    
    void onLogon(const FIX::SessionID& sessionID) override {
        std::cout << "FIX Logon successful: " << sessionID << std::endl;
        session_id_ = sessionID;
        
        // Subscribe to market data
        subscribe_market_data("EUR/USD");
    }
    
    void onLogout(const FIX::SessionID& sessionID) override {
        std::cout << "FIX Logout: " << sessionID << std::endl;
    }
    
    void toAdmin(FIX::Message& message, const FIX::SessionID&) override {}
    void toApp(FIX::Message& message, const FIX::SessionID&) 
        throw(FIX::DoNotSend) override {}
    
    void fromAdmin(const FIX::Message& message, const FIX::SessionID&)
        throw(FIX::FieldNotFound, FIX::IncorrectDataFormat, 
              FIX::IncorrectTagValue, FIX::RejectLogon) override {}
    
    void fromApp(const FIX::Message& message, const FIX::SessionID& sessionID)
        throw(FIX::FieldNotFound, FIX::IncorrectDataFormat,
              FIX::IncorrectTagValue, FIX::UnsupportedMessageType) override {
        crack(message, sessionID);
    }
    
    // Market data handler
    void onMessage(const FIX44::MarketDataSnapshotFullRefresh& message,
                   const FIX::SessionID&) override {
        MarketTick tick;
        tick.timestamp = now();
        
        // Parse FIX market data
        FIX::Symbol symbol;
        message.get(symbol);
        tick.symbol_id = map_symbol(symbol);
        
        // Parse order book levels
        FIX::NoMDEntries numEntries;
        message.get(numEntries);
        
        int bid_count = 0, ask_count = 0;
        
        for (int i = 1; i <= numEntries; ++i) {
            FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
            message.getGroup(i, group);
            
            FIX::MDEntryType entryType;
            FIX::MDEntryPx price;
            FIX::MDEntrySize size;
            
            group.get(entryType);
            group.get(price);
            group.get(size);
            
            if (entryType == FIX::MDEntryType_BID && bid_count < 10) {
                tick.bid_prices[bid_count] = price;
                tick.bid_sizes[bid_count] = static_cast<uint32_t>(size * 1000);
                bid_count++;
            } else if (entryType == FIX::MDEntryType_OFFER && ask_count < 10) {
                tick.ask_prices[ask_count] = price;
                tick.ask_sizes[ask_count] = static_cast<uint32_t>(size * 1000);
                ask_count++;
            }
        }
        
        tick.depth_levels = std::min(bid_count, ask_count);
        
        // Push to market data queue
        market_queue_.try_push(tick);
    }
    
    // Execution report handler
    void onMessage(const FIX44::ExecutionReport& message,
                   const FIX::SessionID&) override {
        FIX::ExecType execType;
        FIX::OrdStatus ordStatus;
        FIX::ClOrdID clOrdID;
        
        message.get(execType);
        message.get(ordStatus);
        message.get(clOrdID);
        
        std::cout << "Execution Report: "
                  << "ClOrdID=" << clOrdID
                  << " ExecType=" << execType
                  << " OrdStatus=" << ordStatus << std::endl;
        
        // Handle fills, partial fills, rejections
        if (ordStatus == FIX::OrdStatus_FILLED) {
            FIX::LastQty lastQty;
            FIX::LastPx lastPx;
            message.get(lastQty);
            message.get(lastPx);
            std::cout << "FILLED: Qty=" << lastQty << " Price=" << lastPx << std::endl;
        }
    }
    
    // Send order
    void send_order(const Order& order) {
        FIX44::NewOrderSingle new_order;
        
        // Set required fields
        new_order.set(FIX::ClOrdID(std::to_string(next_order_id_++)));
        new_order.set(FIX::Symbol(get_symbol_string(order.symbol_id)));
        new_order.set(FIX::Side(order.side == Side::BUY ? 
                                FIX::Side_BUY : FIX::Side_SELL));
        new_order.set(FIX::TransactTime());
        new_order.set(FIX::OrdType(FIX::OrdType_LIMIT));
        new_order.set(FIX::Price(order.price));
        new_order.set(FIX::OrderQty(order.quantity / 1000.0));
        new_order.set(FIX::TimeInForce(FIX::TimeInForce_GOOD_TILL_CANCEL));
        
        try {
            FIX::Session::sendToTarget(new_order, session_id_);
            std::cout << "Order sent: ID=" << order.order_id << std::endl;
        } catch (FIX::SessionNotFound& e) {
            std::cerr << "Session not found: " << e.what() << std::endl;
        }
    }
    
private:
    void subscribe_market_data(const std::string& symbol) {
        FIX44::MarketDataRequest request;
        request.set(FIX::MDReqID("MD_" + std::to_string(++md_req_id_)));
        request.set(FIX::SubscriptionRequestType(
            FIX::SubscriptionRequestType_SNAPSHOT_PLUS_UPDATES));
        request.set(FIX::MarketDepth(10)); // Request 10 levels
        
        FIX44::MarketDataRequest::NoMDEntryTypes group;
        group.set(FIX::MDEntryType(FIX::MDEntryType_BID));
        request.addGroup(group);
        group.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
        request.addGroup(group);
        
        FIX44::MarketDataRequest::NoRelatedSym symbolGroup;
        symbolGroup.set(FIX::Symbol(symbol));
        request.addGroup(symbolGroup);
        
        FIX::Session::sendToTarget(request, session_id_);
    }
    
    int map_symbol(const FIX::Symbol& symbol) {
        // Map FIX symbol to internal ID
        std::string sym = symbol.getValue();
        if (sym == "EUR/USD") return 1;
        if (sym == "GBP/USD") return 2;
        return 0;
    }
    
    std::string get_symbol_string(int symbol_id) {
        if (symbol_id == 1) return "EUR/USD";
        if (symbol_id == 2) return "GBP/USD";
        return "EUR/USD";
    }
    
    LockFreeSPSC<MarketTick, 65536>& market_queue_;
    LockFreeSPSC<Order, 16384>& order_queue_;
    FIX::SessionID session_id_;
    std::atomic<uint64_t> next_order_id_;
    std::atomic<int> md_req_id_{0};
};

#endif // FIX_GATEWAY_HPP
```

---

## 🌐 Popular Broker APIs

### 1. Interactive Brokers (IBKR)

**Technology:** FIX 4.2/4.4, TWS API (Java/C++)

**Latency:** 10-50ms (retail), 1-5ms (co-located)

**Integration:**
```cpp
// Use IB's C++ API
#include "TwsApiC++.h"

class IBGateway : public EWrapper {
    // Implement tick handling, order execution
};
```

**Resources:**
- API: https://interactivebrokers.github.io/
- FIX: https://www.interactivebrokers.com/en/trading/fix-protocol.php

---

### 2. Alpaca (Crypto & US Stocks)

**Technology:** REST + WebSocket

**Latency:** 50-200ms

**Example:**
```cpp
// Market data WebSocket: wss://stream.data.alpaca.markets/v2/iex
// Order API: https://api.alpaca.markets/v2/orders
```

**Resources:** https://alpaca.markets/docs/

---

### 3. Coinbase Advanced Trade

**Technology:** REST + WebSocket

**Example:**
```cpp
// WebSocket: wss://advanced-trade-ws.coinbase.com
// REST: https://api.coinbase.com/api/v3/brokerage/
```

---

### 4. Kraken

**Technology:** REST + WebSocket

**Example:**
```cpp
// WebSocket: wss://ws.kraken.com
// REST: https://api.kraken.com
```

---

### 5. CME/ICE (Futures)

**Technology:** FIX 5.0, Binary protocols (iLink3, ICE Binary)

**Latency:** <1ms (co-located)

**Requirements:**
- Clearing membership or FCM relationship
- Co-location in exchange datacenter
- Certified software

---

## 🔐 Authentication & Security

### API Key Management

```cpp
// Environment variables (recommended)
const char* api_key = std::getenv("BROKER_API_KEY");
const char* api_secret = std::getenv("BROKER_API_SECRET");

// Or encrypted config file
#include <openssl/aes.h>

class SecureConfig {
    std::string decrypt_config(const std::string& encrypted_file) {
        // AES-256 decryption
    }
};
```

### Rate Limiting

```cpp
class RateLimiter {
public:
    RateLimiter(int max_requests_per_second)
        : max_requests_(max_requests_per_second),
          window_start_(now()),
          request_count_(0) {}
    
    bool allow_request() {
        auto current = now();
        if (to_micros(current - window_start_) >= 1'000'000) {
            window_start_ = current;
            request_count_.store(0, std::memory_order_release);
        }
        
        int count = request_count_.fetch_add(1, std::memory_order_acq_rel);
        return count < max_requests_;
    }
    
private:
    int max_requests_;
    Timestamp window_start_;
    std::atomic<int> request_count_;
};
```

---

## 📊 Testing Without Real Money

### 1. Simulated Mode (Current)

Your system already has simulation mode in `kernel_bypass_nic.hpp`:

```cpp
// Generate synthetic market data
void simulate_market_tick() {
    // Already implemented!
}
```

### 2. Paper Trading

Most brokers offer paper trading accounts:

**Binance Testnet:**
```cpp
const std::string testnet_url = "https://testnet.binance.vision";
const std::string testnet_ws = "wss://testnet.binance.vision/ws";
```

**Alpaca Paper:**
```cpp
const std::string paper_url = "https://paper-api.alpaca.markets";
```

### 3. Historical Backtesting

```cpp
class HistoricalReplay {
public:
    void replay_data(const std::string& csv_file) {
        std::ifstream file(csv_file);
        std::string line;
        
        while (std::getline(file, line)) {
            MarketTick tick = parse_csv_line(line);
            
            // Feed to HFT system at historical pace
            market_queue_.try_push(tick);
            
            // Sleep to maintain timing
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100)
            );
        }
    }
};
```

---

## 🚦 Production Deployment Checklist

### Before Going Live

- [ ] **Test on paper trading** for at least 1 week
- [ ] **Verify risk controls** work correctly
- [ ] **Set position limits** appropriate for account size
- [ ] **Configure kill switch** with remote access
- [ ] **Set up monitoring** and alerting
- [ ] **Backup internet connection** (4G/5G failover)
- [ ] **Test reconnection logic** (simulate network failures)
- [ ] **Log all orders** to file and database
- [ ] **Compliance review** (if required by jurisdiction)
- [ ] **Start with small capital** (1-5% of total)

### Monitoring Setup

```cpp
class ProductionMonitor {
public:
    void log_metrics() {
        // Log to file/database every second
        std::ofstream log("trading_metrics.log", std::ios::app);
        log << now() << ","
            << "position=" << current_position_ << ","
            << "pnl=" << current_pnl_ << ","
            << "orders_sent=" << orders_sent_ << ","
            << "orders_filled=" << orders_filled_ << ","
            << "latency_us=" << avg_latency_us_ << "\n";
    }
    
    void check_alerts() {
        // Check for anomalies
        if (current_pnl_ < -10000.0) {
            send_sms_alert("PnL threshold breached!");
            trigger_kill_switch();
        }
    }
};
```

---

## 🔧 Quick Integration Summary

### Minimal Working Example (Binance)

1. **Install dependencies:**
```bash
brew install boost openssl curl
```

2. **Set environment variables:**
```bash
export BINANCE_API_KEY="your_key_here"
export BINANCE_API_SECRET="your_secret_here"
```

3. **Add to main.cpp:**
```cpp
#include "ws_market_data.hpp"
#include "rest_order_gateway.hpp"

// Replace simulated NIC with real WebSocket feed
BinanceWebSocketFeed feed("btcusdt", market_queue);
BinanceOrderGateway gateway(getenv("BINANCE_API_KEY"), 
                           getenv("BINANCE_API_SECRET"));
```

4. **Build and run:**
```bash
./build.sh
./build/hft_system
```

---

## 📚 Additional Resources

- **FIX Protocol:** https://www.fixtrading.org/
- **QuickFIX Documentation:** http://www.quickfixengine.org/
- **Boost.Beast (WebSocket):** https://www.boost.org/doc/libs/release/libs/beast/
- **Interactive Brokers API:** https://interactivebrokers.github.io/
- **Binance API Docs:** https://binance-docs.github.io/apidocs/

---

## ⚠️ Legal Disclaimer

**This system is for educational purposes. Before live trading:**

1. Ensure compliance with local regulations
2. Understand the risks of algorithmic trading
3. Have appropriate capital and risk management
4. Consider consulting with legal/compliance professionals
5. Never risk more than you can afford to lose

**HFT regulations vary by jurisdiction. Some regions require:**
- Trading licenses
- Market maker agreements
- Direct market access approval
- Compliance infrastructure

---

**Questions?** Create detailed issues for specific broker integrations you need.
