#include "../include/mqtt_ws_bridge.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/resource.h>
#include <fstream>

namespace mqtt_ws {

// Global bridge instance for libwebsockets callback
static MqttWebSocketBridge* g_bridge_instance = nullptr;

//=============================================================================
// MessageBuffer Implementation
//=============================================================================

MessageBuffer::MessageBuffer(size_t initial_capacity) 
    : buffer_(initial_capacity), size_(0), capacity_(initial_capacity) {
}

void MessageBuffer::resize(size_t new_size) {
    if (new_size > capacity_) {
        buffer_.resize(new_size);
        capacity_ = new_size;
    }
    size_ = new_size;
}

bool MessageBuffer::parse_websocket_message(std::string& topic, std::vector<uint8_t>& payload) {
    if (size_ < 4) return false; // Minimum for UTF-16LE topic separator
    
    // Parse UTF-16LE encoded topic followed by '|'
    size_t offset = 0;
    topic.clear();
    
    while (offset < size_ - 1) {
        uint16_t char_code = *reinterpret_cast<uint16_t*>(buffer_.data() + offset);
        
        if (char_code == '|') {
            offset += 2;
            break;
        }
        
        // Convert UTF-16LE to ASCII (simplified)
        if (char_code < 128) {
            topic += static_cast<char>(char_code);
        }
        offset += 2;
    }
    
    // Remaining bytes are the payload
    if (offset < size_) {
        payload.assign(buffer_.begin() + offset, buffer_.begin() + size_);
    } else {
        payload.clear();
    }
    
    return !topic.empty();
}

void MessageBuffer::format_mqtt_message(const std::string& topic, const std::vector<uint8_t>& payload) {
    // Format: "topic|" in UTF-16LE + payload
    size_t topic_utf16_size = (topic.length() + 1) * 2; // +1 for '|'
    size_t total_size = topic_utf16_size + payload.size();
    
    resize(total_size);
    
    // Convert topic to UTF-16LE
    uint16_t* utf16_ptr = reinterpret_cast<uint16_t*>(buffer_.data());
    for (size_t i = 0; i < topic.length(); ++i) {
        utf16_ptr[i] = static_cast<uint16_t>(topic[i]);
    }
    utf16_ptr[topic.length()] = '|';
    
    // Copy payload
    if (!payload.empty()) {
        std::memcpy(buffer_.data() + topic_utf16_size, payload.data(), payload.size());
    }
}

//=============================================================================
// WebSocketConnection Implementation
//=============================================================================

WebSocketConnection::WebSocketConnection(struct lws* wsi, const std::string& topic)
    : wsi_(wsi), topic_(topic), active_(false) {
}

WebSocketConnection::~WebSocketConnection() {
    cleanup();
}

bool WebSocketConnection::initialize(const BridgeConfig& config) {
    buffer_ = std::make_unique<MessageBuffer>(config.message_buffer_size);
    
    // Create MQTT client for this connection
    std::string client_id = "ws_client_" + std::to_string(reinterpret_cast<uintptr_t>(wsi_));
    mqtt_client_ = std::make_unique<MqttClient>(client_id, config.mqtt_host, config.mqtt_port);
    
    if (!mqtt_client_->connect()) {
        return false;
    }
    
    // Subscribe to the topic
    if (!mqtt_client_->subscribe(topic_)) {
        return false;
    }
    
    active_ = true;
    return true;
}

void WebSocketConnection::cleanup() {
    if (mqtt_client_) {
        mqtt_client_->disconnect();
        mqtt_client_.reset();
    }
    active_ = false;
}

void WebSocketConnection::handle_websocket_message(const uint8_t* data, size_t len) {
    if (!active_ || !buffer_) return;
    
    // Copy data to buffer for processing
    buffer_->resize(len);
    std::memcpy(buffer_->data(), data, len);
    
    std::string topic;
    std::vector<uint8_t> payload;
    
    if (buffer_->parse_websocket_message(topic, payload)) {
        // Forward to MQTT
        if (mqtt_client_) {
            mqtt_client_->publish(topic, payload);
        }
    }
}

void WebSocketConnection::handle_mqtt_message(const std::string& topic, const std::vector<uint8_t>& payload) {
    if (!active_ || !buffer_) return;
    
    // Format message for WebSocket
    buffer_->format_mqtt_message(topic, payload);
    
    // Send to WebSocket client
    send_to_websocket(std::vector<uint8_t>(buffer_->data(), buffer_->data() + buffer_->size()));
}

bool WebSocketConnection::send_to_websocket(const std::vector<uint8_t>& data) {
    if (!wsi_ || data.empty()) return false;
    
    // This is a simplified implementation - full libwebsockets integration needed
    return true;
}

void WebSocketConnection::close_connection() {
    active_ = false;
    if (wsi_) {
        lws_close_reason(wsi_, LWS_CLOSE_STATUS_NORMAL, nullptr, 0);
    }
}

//=============================================================================
// MqttClient Implementation (Basic)
//=============================================================================

MqttClient::MqttClient(const std::string& client_id, const std::string& host, int port)
    : mosq_(nullptr), client_id_(client_id), host_(host), port_(port), connected_(false) {
    
    mosq_ = mosquitto_new(client_id_.c_str(), true, this);
    if (mosq_) {
        mosquitto_connect_callback_set(mosq_, on_connect_callback);
        mosquitto_message_callback_set(mosq_, on_message_callback);
        mosquitto_disconnect_callback_set(mosq_, on_disconnect_callback);
    }
}

MqttClient::~MqttClient() {
    disconnect();
    if (mosq_) {
        mosquitto_destroy(mosq_);
    }
}

bool MqttClient::connect() {
    if (!mosq_) return false;
    
    int rc = mosquitto_connect(mosq_, host_.c_str(), port_, 60);
    if (rc == MOSQ_ERR_SUCCESS) {
        mosquitto_loop_start(mosq_);
        return true;
    }
    return false;
}

void MqttClient::disconnect() {
    if (mosq_ && connected_) {
        mosquitto_loop_stop(mosq_, false);
        mosquitto_disconnect(mosq_);
    }
    connected_ = false;
}

bool MqttClient::subscribe(const std::string& topic) {
    if (!mosq_ || !connected_) return false;
    return mosquitto_subscribe(mosq_, nullptr, topic.c_str(), 0) == MOSQ_ERR_SUCCESS;
}

bool MqttClient::unsubscribe(const std::string& topic) {
    if (!mosq_ || !connected_) return false;
    return mosquitto_unsubscribe(mosq_, nullptr, topic.c_str()) == MOSQ_ERR_SUCCESS;
}

bool MqttClient::publish(const std::string& topic, const std::vector<uint8_t>& payload, int qos) {
    if (!mosq_ || !connected_) return false;
    return mosquitto_publish(mosq_, nullptr, topic.c_str(), payload.size(), 
                            payload.data(), qos, false) == MOSQ_ERR_SUCCESS;
}

void MqttClient::set_message_callback(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback) {
    message_callback_ = callback;
}

// Static callbacks for mosquitto
void MqttClient::on_connect_callback(struct mosquitto*, void* userdata, int rc) {
    MqttClient* client = static_cast<MqttClient*>(userdata);
    if (rc == 0) {
        client->connected_ = true;
    }
}

void MqttClient::on_message_callback(struct mosquitto*, void* userdata, const struct mosquitto_message* message) {
    MqttClient* client = static_cast<MqttClient*>(userdata);
    if (client->message_callback_) {
        std::vector<uint8_t> payload(static_cast<uint8_t*>(message->payload), 
                                   static_cast<uint8_t*>(message->payload) + message->payloadlen);
        client->message_callback_(std::string(message->topic), payload);
    }
}

void MqttClient::on_disconnect_callback(struct mosquitto*, void* userdata, int) {
    MqttClient* client = static_cast<MqttClient*>(userdata);
    client->connected_ = false;
}

//=============================================================================
// MqttWebSocketBridge Implementation
//=============================================================================

MqttWebSocketBridge::MqttWebSocketBridge(const BridgeConfig& config)
    : config_(config), lws_context_(nullptr), connection_count_(0), 
      running_(false), epoll_fd_(-1), events_(nullptr), ssl_ctx_(nullptr) {
    
    g_bridge_instance = this;
    
    // Initialize mosquitto library
    mosquitto_lib_init();
    
    std::cout << "🔧 Initializing MQTT-WebSocket Bridge..." << std::endl;
    std::cout << "   MQTT Broker: " << config_.mqtt_host << ":" << config_.mqtt_port << std::endl;
    std::cout << "   WebSocket Port: " << config_.websocket_port << std::endl;
    std::cout << "   Worker Threads: " << config_.worker_threads << std::endl;
}

MqttWebSocketBridge::~MqttWebSocketBridge() {
    stop();
    cleanup_resources();
    mosquitto_lib_cleanup();
    g_bridge_instance = nullptr;
}

bool MqttWebSocketBridge::initialize() {
    std::cout << "🚀 Initializing bridge components..." << std::endl;
    
    // Setup SSL if certificates are provided
    if (!config_.ssl_cert_path.empty() && !config_.ssl_key_path.empty()) {
        if (!setup_ssl_context()) {
            std::cerr << "❌ Failed to setup SSL context" << std::endl;
            return false;
        }
        std::cout << "✅ SSL context initialized" << std::endl;
    }
    
    // Setup libwebsockets
    if (!setup_libwebsockets()) {
        std::cerr << "❌ Failed to setup libwebsockets" << std::endl;
        return false;
    }
    std::cout << "✅ WebSocket server initialized" << std::endl;
    
    // Setup epoll for I/O (Linux optimization)
    if (config_.use_epoll && !setup_epoll()) {
        std::cerr << "❌ Failed to setup epoll" << std::endl;
        return false;
    }
    
    std::cout << "✅ Bridge initialization complete" << std::endl;
    return true;
}

bool MqttWebSocketBridge::start() {
    if (running_.load()) {
        std::cout << "⚠️  Bridge is already running" << std::endl;
        return true;
    }
    
    running_ = true;
    
    std::cout << "🌐 Starting WebSocket server on port " << config_.websocket_port << std::endl;
    
    // Start single worker thread for libwebsockets (thread-safe approach)
    worker_threads_.emplace_back(&MqttWebSocketBridge::worker_thread_loop, this);
    
    std::cout << "✅ Bridge started successfully!" << std::endl;
    std::cout << "📊 Monitoring connections..." << std::endl;
    
    return true;
}

void MqttWebSocketBridge::stop() {
    if (!running_.load()) return;
    
    std::cout << "\n🛑 Stopping bridge..." << std::endl;
    running_ = false;
    
    // Wait for worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    cleanup_connections();
    std::cout << "✅ Bridge stopped gracefully" << std::endl;
}

//=============================================================================
// Private Method Implementations  
//=============================================================================

bool MqttWebSocketBridge::setup_ssl_context() {
    // Basic SSL setup - can be enhanced later
    ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx_) return false;
    
    if (SSL_CTX_use_certificate_file(ssl_ctx_, config_.ssl_cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        return false;
    }
    
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, config_.ssl_key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        return false;
    }
    
    return true;
}

bool MqttWebSocketBridge::setup_libwebsockets() {
    // Define WebSocket protocols
    static struct lws_protocols protocols[] = {
        {
            "mqtt-ws-protocol",                     // name
            websocket_callback,                     // callback
            0,                                      // per_session_data_size
            1024,                                  // rx_buffer_size
        },
        { NULL, NULL, 0, 0 } // terminator
    };
    
    // Initialize libwebsockets context info
    memset(&lws_info_, 0, sizeof(lws_info_));
    lws_info_.port = config_.websocket_port;
    lws_info_.iface = config_.websocket_host.c_str();
    lws_info_.protocols = protocols;
    lws_info_.gid = -1;
    lws_info_.uid = -1;
    
    // SSL configuration
    if (ssl_ctx_) {
        lws_info_.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        lws_info_.ssl_cert_filepath = config_.ssl_cert_path.c_str();
        lws_info_.ssl_private_key_filepath = config_.ssl_key_path.c_str();
    }
    
    // Create context
    lws_context_ = lws_create_context(&lws_info_);
    return lws_context_ != nullptr;
}

bool MqttWebSocketBridge::setup_epoll() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) return false;
    
    events_ = new struct epoll_event[config_.max_connections];
    return true;
}

void MqttWebSocketBridge::worker_thread_loop() {
    std::cout << "🔄 Main worker thread started (ID: " << std::this_thread::get_id() << ")" << std::endl;
    
    while (running_.load()) {
        if (lws_context_) {
            // Service libwebsockets with longer timeout for stability
            int n = lws_service(lws_context_, 1000); // 1 second timeout
            if (n < 0) {
                std::cerr << "⚠️  lws_service returned error: " << n << std::endl;
                break;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "🏁 Main worker thread finished (ID: " << std::this_thread::get_id() << ")" << std::endl;
}

void MqttWebSocketBridge::cleanup_connections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.clear();
    connection_count_ = 0;
}

void MqttWebSocketBridge::cleanup_resources() {
    if (lws_context_) {
        lws_context_destroy(lws_context_);
        lws_context_ = nullptr;
    }
    
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
    
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    if (events_) {
        delete[] events_;
        events_ = nullptr;
    }
}

void MqttWebSocketBridge::handle_new_connection(struct lws* wsi, const std::string& topic) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto connection = std::make_unique<WebSocketConnection>(wsi, topic);
    if (connection->initialize(config_)) {
        connections_[wsi] = std::move(connection);
        connection_count_++;
        std::cout << "✅ New connection initialized for topic: " << topic << " (Total: " << connection_count_ << ")" << std::endl;
    } else {
        std::cout << "❌ Failed to initialize connection for topic: " << topic << std::endl;
    }
}

void MqttWebSocketBridge::handle_connection_close(struct lws* wsi) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(wsi);
    if (it != connections_.end()) {
        std::cout << "🗑️  Removing connection for topic: " << it->second->get_topic() << std::endl;
        connections_.erase(it);
        connection_count_--;
        std::cout << "✅ Connection removed (Total: " << connection_count_ << ")" << std::endl;
    }
}

void MqttWebSocketBridge::process_websocket_message(struct lws* wsi, const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(wsi);
    if (it != connections_.end()) {
        it->second->handle_websocket_message(data, len);
    }
}

// Static callback for libwebsockets
int MqttWebSocketBridge::websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                           void* user, void* in, size_t len) {
    // Get bridge instance
    MqttWebSocketBridge* bridge = g_bridge_instance;
    if (!bridge) return -1;
    
    switch (reason) {
        case LWS_CALLBACK_FILTER_NETWORK_CONNECTION: {
            // Allow all connections for now
            std::cout << "🌐 Network connection filter" << std::endl;
            return 0;
        }
        
        case LWS_CALLBACK_FILTER_HTTP_CONNECTION: {
            // Allow HTTP connections for WebSocket upgrade
            std::cout << "🔗 HTTP connection filter" << std::endl;
            return 0;
        }
        
        case LWS_CALLBACK_ESTABLISHED: {
            // New WebSocket connection
            std::cout << "📱 New WebSocket connection established" << std::endl;
            
            // Extract topic from URL path (simplified)
            std::string topic = "test/topic"; // In real implementation, parse from URL
            bridge->handle_new_connection(wsi, topic);
            break;
        }
        
        case LWS_CALLBACK_RECEIVE: {
            // Message received from WebSocket client
            std::cout << "📨 Message received (" << len << " bytes)" << std::endl;
            if (in && len > 0) {
                bridge->process_websocket_message(wsi, static_cast<const uint8_t*>(in), len);
            }
            break;
        }
        
        case LWS_CALLBACK_CLOSED: {
            // Connection closed
            std::cout << "🔌 WebSocket connection closed" << std::endl;
            bridge->handle_connection_close(wsi);
            break;
        }
        
        case LWS_CALLBACK_HTTP: {
            // HTTP request - check if it's a WebSocket upgrade request
            const char *requested_uri = (char *)in;
            
            std::cout << "📡 HTTP request for: " << (requested_uri ? requested_uri : "null") << std::endl;
            
            // For WebSocket upgrade requests, allow the upgrade
            if (lws_hdr_total_length(wsi, WSI_TOKEN_UPGRADE) > 0) {
                std::cout << "🔄 WebSocket upgrade request detected" << std::endl;
                return 0; // Allow upgrade
            }
            
            // For regular HTTP requests, return simple 404
            lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, "WebSocket endpoint only");
            return -1;
        }
        
        default:
            break;
    }
    
    // Suppress unused parameter warnings
    (void)user;
    
    return 0;
}

} // namespace mqtt_ws
