#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include "metrics_collector.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <set>
#include <iostream>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// WebSocket session for each connected client
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket, hft::profiling::MetricsCollector& collector)
        : ws_(std::move(socket)), collector_(collector) {}
    
    void run() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                self->read_message();
            }
        });
    }
    
    void send_metrics(const std::string& json_data) {
        auto msg = std::make_shared<std::string>(json_data);
        ws_.async_write(
            net::buffer(*msg),
            [self = shared_from_this(), msg](beast::error_code ec, std::size_t) {
                // Message sent
            }
        );
    }
    
private:
    void read_message() {
        ws_.async_read(
            buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
                if (!ec) {
                    self->handle_message();
                    self->read_message();
                }
            }
        );
    }
    
    void handle_message() {
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        // Handle client requests (e.g., "get_history", "get_summary")
        try {
            auto j = json::parse(msg);
            std::string cmd = j.value("command", "");
            
            if (cmd == "get_history") {
                send_history();
            } else if (cmd == "get_summary") {
                send_summary();
            }
        } catch (...) {
            // Ignore malformed messages
        }
    }
    
    void send_history() {
        // TODO: Implement snapshot history
        // auto snapshots = collector_.get_recent_snapshots(1000);
        json j = json::array();
        
        // Placeholder empty history
        j.push_back({
            {"timestamp", 0},
            {"mid_price", 0.0},
            {"spread", 0.0},
            {"pnl", 0.0},
            {"position", 0},
            {"buy_intensity", 0.0},
            {"sell_intensity", 0.0},
            {"latency", 0.0}
        });
        
        send_metrics(j.dump());
    }
    
    void send_summary() {
        // TODO: Implement summary stats
        // auto stats = collector_.get_summary();
        json j = {
            {"type", "summary"},
            {"avg_pnl", 0.0},
            {"max_pnl", 0.0},
            {"min_pnl", 0.0},
            {"avg_latency", 0.0},
            {"max_latency", 0.0},
            {"total_trades", 0},
            {"fill_rate", 0.0}
        };
        
        send_metrics(j.dump());
    }
    
    websocket::stream<tcp::socket> ws_;
    hft::profiling::MetricsCollector& collector_;
    beast::flat_buffer buffer_;
};

// WebSocket server for dashboard
class DashboardServer {
public:
    DashboardServer(hft::profiling::MetricsCollector& collector, int port = 8080)
        : collector_(collector),
          ioc_(),
          acceptor_(ioc_, tcp::endpoint(tcp::v4(), port)),
          running_(false) {}
    
    void start() {
        running_.store(true, std::memory_order_release);
        
        // Start accepting connections
        server_thread_ = std::thread([this]() {
            accept_connection();
            ioc_.run();
        });
        
        // Start metrics broadcast thread
        broadcast_thread_ = std::thread([this]() {
            broadcast_metrics();
        });
        
        std::cout << "Dashboard server started on port " 
                  << acceptor_.local_endpoint().port() << std::endl;
        std::cout << "Open http://localhost:" << acceptor_.local_endpoint().port() 
                  << " in your browser" << std::endl;
    }
    
    void stop() {
        running_.store(false, std::memory_order_release);
        ioc_.stop();
        
        if (server_thread_.joinable()) server_thread_.join();
        if (broadcast_thread_.joinable()) broadcast_thread_.join();
    }
    
private:
    void accept_connection() {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<WebSocketSession>(
                    std::move(socket), collector_
                );
                
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.insert(session);
                session->run();
            }
            
            if (running_.load(std::memory_order_acquire)) {
                accept_connection();
            }
        });
    }
    
    void broadcast_metrics() {
        while (running_.load(std::memory_order_acquire)) {
            // Broadcast every 100ms
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // TODO: Implement proper metrics structure
            // auto& metrics = collector_.get_metrics();
            
            json j = {
                {"type", "update"},
                {"timestamp", std::chrono::steady_clock::now().time_since_epoch().count()},
                {"mid_price", 0.0}, // TODO: Wire up actual metrics
                {"spread", 0.0},
                {"pnl", 0.0},
                {"position", 0},
                {"buy_intensity", 0.0},
                {"sell_intensity", 0.0},
                {"latency", 0.0},
                {"orders_sent", 0},
                {"orders_filled", 0},
                {"regime", 0},
                {"position_usage", 0.0}
            };
            
            std::string msg = j.dump();
            
            // Send to all connected clients
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            for (auto& session : sessions_) {
                session->send_metrics(msg);
            }
        }
    }
    
    hft::profiling::MetricsCollector& collector_;
    net::io_context ioc_;
    tcp::acceptor acceptor_;
    std::atomic<bool> running_;
    
    std::set<std::shared_ptr<WebSocketSession>> sessions_;
    std::mutex sessions_mutex_;
    
    std::thread server_thread_;
    std::thread broadcast_thread_;
};

#endif // WEBSOCKET_SERVER_HPP
