#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <sys/epoll.h>
#include <libwebsockets.h>
#include <mosquitto.h>
#include <openssl/ssl.h>
#include "../../../thermal-monitoring/ThermalIsolationTracker.h"

namespace mqtt_ws {

/**
 * MQTT-WebSocket Bridge
 * Optimized for Linux with epoll and zero-copy message handling
 */

struct BridgeConfig {
    // MQTT Configuration
    std::string mqtt_host = "localhost";
    int mqtt_port = 1883;
    int mqtt_keepalive = 60;
    
    // WebSocket Configuration  
    std::string websocket_host = "0.0.0.0";
    int websocket_port = 8080;
    std::string ssl_cert_path;
    std::string ssl_key_path;
    
    // Connection Settings
    int max_connections = 1000;
    int worker_threads = std::thread::hardware_concurrency();
    int message_buffer_size = 4096;
    int connection_timeout = 30;
    
    // Optimization flags
    bool use_epoll = true;
    bool zero_copy_enabled = true;
    bool connection_pooling = true;
    
    // Thermal Monitoring Configuration
    bool thermal_monitoring_enabled = true;
    thermal_monitoring::ThermalConfig thermal_config;
};

// Forward declarations
class MqttClient;
class WebSocketConnection;
class MessageBuffer;

/**
 * Message buffer with zero-copy optimization
 */
class MessageBuffer {
private:
    std::vector<uint8_t> buffer_;
    size_t size_;
    size_t capacity_;
    
public:
    MessageBuffer(size_t initial_capacity = 4096);
    ~MessageBuffer() = default;
    
    // Zero-copy operations
    uint8_t* data() { return buffer_.data(); }
    const uint8_t* data() const { return buffer_.data(); }
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    
    void resize(size_t new_size);
    void clear() { size_ = 0; }
    
    // Efficient topic/message parsing
    bool parse_websocket_message(std::string& topic, std::vector<uint8_t>& payload);
    void format_mqtt_message(const std::string& topic, const std::vector<uint8_t>& payload);
};

/**
 * MQTT Client wrapper with connection pooling
 */
class MqttClient {
private:
    struct mosquitto* mosq_;
    std::string client_id_;
    std::string host_;
    int port_;
    std::atomic<bool> connected_;
    std::function<void(const std::string&, const std::vector<uint8_t>&)> message_callback_;
    
public:
    MqttClient(const std::string& client_id, const std::string& host, int port);
    ~MqttClient();
    
    bool connect();
    void disconnect();
    bool is_connected() const { return connected_.load(); }
    
    bool subscribe(const std::string& topic);
    bool unsubscribe(const std::string& topic);
    bool publish(const std::string& topic, const std::vector<uint8_t>& payload, int qos = 0);
    
    void set_message_callback(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback);
    
    // Static callbacks for mosquitto
    static void on_connect_callback(struct mosquitto*, void*, int);
    static void on_message_callback(struct mosquitto*, void*, const struct mosquitto_message*);
    static void on_disconnect_callback(struct mosquitto*, void*, int);
};

/**
 * WebSocket Connection with optimized message handling
 */
class WebSocketConnection {
private:
    struct lws* wsi_;
    std::string topic_;
    std::unique_ptr<MqttClient> mqtt_client_;
    std::unique_ptr<MessageBuffer> buffer_;
    std::atomic<bool> active_;
    std::string client_address_;
    
public:
    WebSocketConnection(struct lws* wsi, const std::string& topic);
    ~WebSocketConnection();
    
    bool initialize(const BridgeConfig& config);
    void cleanup();
    
    // Message handling
    void handle_websocket_message(const uint8_t* data, size_t len);
    void handle_mqtt_message(const std::string& topic, const std::vector<uint8_t>& payload);
    
    // Connection management
    bool is_active() const { return active_.load(); }
    const std::string& get_topic() const { return topic_; }
    const std::string& get_client_address() const { return client_address_; }
    
    // WebSocket operations
    bool send_to_websocket(const std::vector<uint8_t>& data);
    void close_connection();
};

/**
 * Main Bridge class with Linux optimization
 */
class MqttWebSocketBridge {
private:
    BridgeConfig config_;
    
    // LibWebSockets context
    struct lws_context* lws_context_;
    struct lws_context_creation_info lws_info_;
    
    // Connection management
    std::unordered_map<struct lws*, std::unique_ptr<WebSocketConnection>> connections_;
    std::mutex connections_mutex_;
    std::atomic<int> connection_count_;
    
    // Threading
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_;
    
    // Epoll for I/O
    int epoll_fd_;
    struct epoll_event* events_;
    
    // SSL context
    SSL_CTX* ssl_ctx_;
    
    // Thermal monitoring
    std::unique_ptr<thermal_monitoring::ThermalIsolationTracker> thermal_tracker_;
    
public:
    MqttWebSocketBridge(const BridgeConfig& config);
    ~MqttWebSocketBridge();
    
    // Main interface
    bool initialize();
    bool start();
    void stop();
    
    // Connection callbacks (static for libwebsockets)
    static int websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                 void* user, void* in, size_t len);
    
    // Sensor message processing (public for integration)
    void process_sensor_message(const std::string& topic, const std::string& payload);
    
private:
    // Internal methods
    bool setup_ssl_context();
    bool setup_libwebsockets();
    bool setup_epoll();
    
    void worker_thread_loop();
    void handle_new_connection(struct lws* wsi, const std::string& topic);
    void handle_connection_close(struct lws* wsi);
    
    // Message processing
    void process_websocket_message(struct lws* wsi, const uint8_t* data, size_t len);
    
    // Thermal monitoring
    bool setup_thermal_monitoring();
    void handle_thermal_alert(const thermal_monitoring::Alert& alert);
    
    // Utility methods
    std::string extract_topic_from_url(const std::string& url);
    std::string get_client_address(struct lws* wsi);
    
    // Cleanup
    void cleanup_connections();
    void cleanup_resources();
};

/**
 * Utility functions
 */
namespace utils {
    std::string url_decode(const std::string& encoded);
    std::vector<uint8_t> string_to_bytes(const std::string& str);
    std::string bytes_to_string(const std::vector<uint8_t>& bytes);
    
    uint64_t get_timestamp_us();
}

} // namespace mqtt_ws 