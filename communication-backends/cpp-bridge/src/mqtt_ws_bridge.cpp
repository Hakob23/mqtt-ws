#include "../include/mqtt_ws_bridge.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
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
    if (size_ == 0) return false;
    
    // Convert message to string for parsing (UTF-8)
    std::string message_str(buffer_.begin(), buffer_.begin() + size_);
    
    std::cout << "🔍 [C++] Parsing message: '" << message_str << "'" << std::endl;
    
    // Find the topic separator '|'
    size_t pipe_pos = message_str.find('|');
    if (pipe_pos == std::string::npos) {
        std::cout << "❌ [C++] Invalid message format - no topic separator found" << std::endl;
        return false;
    }
    
    // Extract topic and payload
    topic = message_str.substr(0, pipe_pos);
    std::string payload_str = message_str.substr(pipe_pos + 1);
    
    // Convert payload string to bytes
    payload.assign(payload_str.begin(), payload_str.end());
    
    std::cout << "✅ [C++] Parsed topic: '" << topic << "', payload: '" << payload_str << "'" << std::endl;
    return !topic.empty();
}

void MessageBuffer::format_mqtt_message(const std::string& topic, const std::vector<uint8_t>& payload) {
    // Format: "topic|payload" in UTF-8
    std::string payload_str(payload.begin(), payload.end());
    std::string message_str = topic + "|" + payload_str;
    
    std::cout << "📝 [C++] Formatting message: '" << message_str << "'" << std::endl;
    
    resize(message_str.size());
    std::memcpy(buffer_.data(), message_str.data(), message_str.size());
}

//=============================================================================
// WebSocketConnection Implementation
//=============================================================================

WebSocketConnection::WebSocketConnection(struct lws* wsi, const std::string& topic)
    : wsi_(wsi), topic_(topic), active_(false) {
}

WebSocketConnection::~WebSocketConnection() {
    std::cout << "🔍 DEBUG: WebSocketConnection destructor called" << std::endl;
    std::cout.flush();
    
    cleanup();
    
    std::cout << "🔍 DEBUG: WebSocketConnection destructor completed" << std::endl;
    std::cout.flush();
}

bool WebSocketConnection::initialize(const BridgeConfig& config) {
    buffer_ = std::make_unique<MessageBuffer>(config.message_buffer_size);
    
    // Create MQTT client for this connection
    std::string client_id = "ws_client_" + std::to_string(reinterpret_cast<uintptr_t>(wsi_));
    mqtt_client_ = std::make_unique<MqttClient>(client_id, config.mqtt_host, config.mqtt_port);
    
    // Set up the callback to handle incoming MQTT messages
    mqtt_client_->set_message_callback([this](const std::string& topic, const std::vector<uint8_t>& payload) {
        std::cout << "📩 [C++] Received MQTT message on topic '" << topic << "': " << std::string(payload.begin(), payload.end()) << std::endl;
        this->handle_mqtt_message(topic, payload);
    });
    
    if (!mqtt_client_->connect()) {
        return false;
    }
    
    // Subscribe to the topic
    if (!mqtt_client_->subscribe(topic_)) {
        return false;
    }
    
    std::cout << "✅ [C++] Subscribed to topic: " << topic_ << std::endl;
    active_ = true;
    return true;
}

void WebSocketConnection::cleanup() {
    std::cout << "🔍 DEBUG: WebSocketConnection::cleanup() called" << std::endl;
    std::cout.flush();
    
    if (mqtt_client_) {
        std::cout << "🔍 DEBUG: Calling mqtt_client_->disconnect()" << std::endl;
        std::cout.flush();
        
        mqtt_client_->disconnect();
        
        std::cout << "🔍 DEBUG: Resetting mqtt_client_" << std::endl;
        std::cout.flush();
        
        mqtt_client_.reset();
        
        std::cout << "🔍 DEBUG: mqtt_client_ reset completed" << std::endl;
        std::cout.flush();
    }
    
    active_ = false;
    
    std::cout << "🔍 DEBUG: WebSocketConnection::cleanup() completed" << std::endl;
    std::cout.flush();
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
            std::string payload_str(payload.begin(), payload.end());
            std::cout << "📤 [C++] Publishing to MQTT topic '" << topic << "': '" << payload_str << "'" << std::endl;
            
            if (mqtt_client_->publish(topic, payload)) {
                std::cout << "✅ [C++] Successfully published to MQTT" << std::endl;
            } else {
                std::cout << "❌ [C++] Failed to publish to MQTT" << std::endl;
            }
        }
    }
}

void WebSocketConnection::handle_mqtt_message(const std::string& topic, const std::vector<uint8_t>& payload) {
    if (!active_ || !buffer_) return;
    
    // Check if this is a sensor message that should be processed by thermal monitoring
    std::string payload_str(payload.begin(), payload.end());
    if (topic.find("sensors/") == 0 && g_bridge_instance) {
        // Process sensor message through thermal monitoring
        g_bridge_instance->process_sensor_message(topic, payload_str);
    }
    
    // Format message for WebSocket
    buffer_->format_mqtt_message(topic, payload);
    
    // Send to WebSocket client  
    send_to_websocket(std::vector<uint8_t>(buffer_->data(), buffer_->data() + buffer_->size()));
}

bool WebSocketConnection::send_to_websocket(const std::vector<uint8_t>& data) {
    if (!wsi_ || data.empty()) return false;
    
    std::cout << "📤 [C++] Sending to WebSocket client (" << data.size() << " bytes)" << std::endl;
    
    // Allocate buffer with LWS_PRE bytes padding for libwebsockets
    size_t total_size = LWS_PRE + data.size();
    std::vector<uint8_t> send_buffer(total_size);
    
    // Copy data after the LWS_PRE padding
    std::memcpy(send_buffer.data() + LWS_PRE, data.data(), data.size());
    
    // Send the data using libwebsockets as text (not binary)
    int result = lws_write(wsi_, send_buffer.data() + LWS_PRE, data.size(), LWS_WRITE_TEXT);
    
    if (result < 0) {
        std::cout << "❌ [C++] Failed to send WebSocket message" << std::endl;
        return false;
    }
    
    std::cout << "✅ [C++] Successfully sent " << result << " bytes to WebSocket client" << std::endl;
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
    if (!mosq_) {
        std::cout << "❌ [C++] MQTT client not initialized" << std::endl;
        return false;
    }
    
    std::cout << "🔗 [C++] Connecting to MQTT broker " << host_ << ":" << port_ << std::endl;
    
    int rc = mosquitto_connect(mosq_, host_.c_str(), port_, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cout << "❌ [C++] Failed to connect to MQTT broker, error: " << rc << std::endl;
        return false;
    }
    
    mosquitto_loop_start(mosq_);
    
    // Wait for connection to be established (up to 5 seconds)
    int wait_count = 0;
    while (!connected_ && wait_count < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    
    if (connected_) {
        std::cout << "✅ [C++] Connected to MQTT broker" << std::endl;
        return true;
    } else {
        std::cout << "❌ [C++] Connection to MQTT broker timed out" << std::endl;
        return false;
    }
}

void MqttClient::disconnect() {
    std::cout << "🔍 DEBUG: MqttClient::disconnect() called" << std::endl;
    std::cout.flush();
    
    if (mosq_ && connected_) {
        std::cout << "🔍 DEBUG: Stopping MQTT loop" << std::endl;
        std::cout.flush();
        
        // Force stop the loop to avoid hanging
        mosquitto_loop_stop(mosq_, true);
        
        std::cout << "🔍 DEBUG: Disconnecting from MQTT broker" << std::endl;
        std::cout.flush();
        
        mosquitto_disconnect(mosq_);
        
        std::cout << "🔍 DEBUG: MQTT disconnect completed" << std::endl;
        std::cout.flush();
    }
    
    connected_ = false;
    
    std::cout << "🔍 DEBUG: MqttClient::disconnect() completed" << std::endl;
    std::cout.flush();
}

bool MqttClient::subscribe(const std::string& topic) {
    if (!mosq_) {
        std::cout << "❌ [C++] MQTT client not initialized for subscription" << std::endl;
        return false;
    }
    if (!connected_) {
        std::cout << "❌ [C++] MQTT client not connected for subscription" << std::endl;
        return false;
    }
    
    std::cout << "📝 [C++] Subscribing to MQTT topic: " << topic << std::endl;
    int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), 0);
    if (rc == MOSQ_ERR_SUCCESS) {
        std::cout << "✅ [C++] Successfully subscribed to topic: " << topic << std::endl;
        return true;
    } else {
        std::cout << "❌ [C++] Failed to subscribe to topic, error: " << rc << std::endl;
        return false;
    }
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
        std::cout << "✅ [C++] MQTT connection established successfully" << std::endl;
    } else {
        std::cout << "❌ [C++] MQTT connection failed with code: " << rc << std::endl;
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
    
    // Initialize thermal monitoring if enabled
    if (config_.thermal_monitoring_enabled) {
        thermal_tracker_ = std::make_unique<thermal_monitoring::ThermalIsolationTracker>(config_.thermal_config);
        std::cout << "🌡️  Thermal monitoring initialized" << std::endl;
    }
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
    
    // Setup thermal monitoring
    if (config_.thermal_monitoring_enabled && !setup_thermal_monitoring()) {
        std::cerr << "❌ Failed to setup thermal monitoring" << std::endl;
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
    
    // Start thermal monitoring if enabled
    if (config_.thermal_monitoring_enabled && thermal_tracker_) {
        thermal_tracker_->start();
        std::cout << "🌡️  Thermal monitoring started" << std::endl;
    }
    
    std::cout << "✅ Bridge started successfully!" << std::endl;
    std::cout << "📊 Monitoring connections..." << std::endl;
    
    return true;
}

void MqttWebSocketBridge::stop() {
    if (!running_.load()) return;
    
    std::cout << "\n🛑 Stopping bridge..." << std::endl;
    running_ = false;
    
    // Stop thermal monitoring if running
    if (thermal_tracker_) {
        thermal_tracker_->stop();
        std::cout << "🌡️  Thermal monitoring stopped" << std::endl;
    }
    
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
    
    std::cout << "🔍 DEBUG: handle_connection_close called for wsi=" << wsi << std::endl;
    std::cout.flush();
    
    auto it = connections_.find(wsi);
    if (it != connections_.end()) {
        std::string topic = it->second->get_topic();
        std::cout << "🔍 DEBUG: Found connection in map for topic: " << topic << std::endl;
        std::cout.flush();
        
        std::cout << "🗑️  Removing connection for topic: " << topic << std::endl;
        std::cout.flush(); // Ensure first message is shown
        
        std::cout << "🔍 DEBUG: About to erase connection from map" << std::endl;
        std::cout.flush();
        
        connections_.erase(it);
        connection_count_--;
        
        std::cout << "🔍 DEBUG: Connection erased, new count: " << connection_count_ << std::endl;
        std::cout.flush();
        
        std::cout << "✅ Connection removed (Total: " << connection_count_ << ")" << std::endl;
        std::cout.flush(); // Ensure second message is shown
        
        std::cout << "🔍 DEBUG: handle_connection_close completed successfully" << std::endl;
        std::cout.flush();
    } else {
        std::cout << "⚠️  Attempted to remove unknown connection (wsi=" << wsi << ")" << std::endl;
        std::cout.flush();
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

//=============================================================================
// Thermal Monitoring Integration
//=============================================================================

bool MqttWebSocketBridge::setup_thermal_monitoring() {
    if (!thermal_tracker_) {
        return false;
    }
    
    // Set up alert callback to handle thermal alerts
    thermal_tracker_->set_alert_callback(
        [this](const thermal_monitoring::Alert& alert) {
            this->handle_thermal_alert(alert);
        }
    );
    
    std::cout << "✅ Thermal monitoring setup complete" << std::endl;
    return true;
}

void MqttWebSocketBridge::process_sensor_message(const std::string& topic, const std::string& payload) {
    if (!thermal_tracker_) return;
    
    // Parse sensor message using the thermal monitoring utilities
    auto sensor_reading = thermal_monitoring::parse_sensor_message(topic, payload);
    if (sensor_reading) {
        // Process the sensor data through the thermal tracker
        thermal_tracker_->process_sensor_data(
            sensor_reading->sensor_id,
            sensor_reading->temperature,
            sensor_reading->humidity,
            sensor_reading->location
        );
    }
}

void MqttWebSocketBridge::handle_thermal_alert(const thermal_monitoring::Alert& alert) {
    // Create MQTT topic for alerts
    std::string alert_topic = "alerts/" + alert.sensor_id;
    
    // Create alert message
    std::stringstream alert_msg;
    alert_msg << "{"
              << "\"sensor_id\":\"" << alert.sensor_id << "\","
              << "\"alert_type\":" << static_cast<int>(alert.alert_type) << ","
              << "\"message\":\"" << alert.message << "\","
              << "\"location\":\"" << alert.location << "\","
              << "\"temperature\":" << alert.temperature << ","
              << "\"humidity\":" << alert.humidity << ","
              << "\"temp_rate\":" << alert.temp_rate << ","
              << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
                     alert.timestamp.time_since_epoch()).count()
              << "}";
    
    std::string alert_json = alert_msg.str();
    
    // Send alert to all connected WebSocket clients
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (const auto& connection_pair : connections_) {
        if (connection_pair.second && connection_pair.second->is_active()) {
            // Format alert for WebSocket transmission
            std::string ws_message = alert_topic + "|" + alert_json;
            std::vector<uint8_t> ws_data(ws_message.begin(), ws_message.end());
            connection_pair.second->send_to_websocket(ws_data);
        }
    }
    
    std::cout << "🚨 Alert sent to " << connections_.size() << " WebSocket clients" << std::endl;
}

} // namespace mqtt_ws
