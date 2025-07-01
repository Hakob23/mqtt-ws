#include "ws_only_bridge.h"
#include "../ThermalIsolationTracker.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <random>

using namespace thermal_monitoring;

// Global bridge instance for LibWebSockets callbacks
static WebSocketOnlyBridge* g_ws_bridge_instance = nullptr;

//=============================================================================
// WebSocketOnlyBridge Implementation
//=============================================================================

WebSocketOnlyBridge::WebSocketOnlyBridge(const Config& config) 
    : config_(config), lws_context_(nullptr), running_(false), 
      server_status_(ServerStatus::STOPPED), stop_processing_(false),
      thermal_enabled_(false) {
    metrics_.start_time = std::chrono::steady_clock::now();
    g_ws_bridge_instance = this;
    
    // Create default room
    if (!config_.default_room.empty()) {
        rooms_[config_.default_room] = std::make_unique<RoomInfo>(config_.default_room);
        rooms_[config_.default_room]->description = "Default room for general communication";
        metrics_.rooms_created++;
    }
}

WebSocketOnlyBridge::~WebSocketOnlyBridge() {
    Stop();
    CleanupLibWebSockets();
    g_ws_bridge_instance = nullptr;
}

bool WebSocketOnlyBridge::Start() {
    if (running_.load()) {
        LogWarning("WebSocket bridge already running");
        return true;
    }
    
    LogInfo("Starting WebSocket-Only Bridge...");
    UpdateServerStatus(ServerStatus::STARTING);
    
    if (!InitializeLibWebSockets()) {
        LogError("Failed to initialize LibWebSockets");
        UpdateServerStatus(ServerStatus::ERROR);
        return false;
    }
    
    // Start worker threads
    stop_processing_ = false;
    for (int i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.emplace_back(std::make_unique<std::thread>(&WebSocketOnlyBridge::WorkerLoop, this));
    }
    
    // Start metrics thread
    metrics_thread_ = std::make_unique<std::thread>(&WebSocketOnlyBridge::MetricsLoop, this);
    
    // Start heartbeat thread if enabled
    if (config_.enable_heartbeat) {
        heartbeat_thread_ = std::make_unique<std::thread>(&WebSocketOnlyBridge::HeartbeatLoop, this);
    }
    
    // Start server thread
    server_thread_ = std::make_unique<std::thread>(&WebSocketOnlyBridge::ServerLoop, this);
    
    running_ = true;
    UpdateServerStatus(ServerStatus::RUNNING);
    LogInfo("WebSocket-Only Bridge started successfully on " + config_.host + ":" + std::to_string(config_.port));
    return true;
}

bool WebSocketOnlyBridge::Stop() {
    if (!running_.load()) {
        return true;
    }
    
    LogInfo("Stopping WebSocket-Only Bridge...");
    UpdateServerStatus(ServerStatus::STOPPING);
    running_ = false;
    stop_processing_ = true;
    
    // Notify all threads to stop
    queue_cv_.notify_all();
    
    // Wait for server thread
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    // Wait for worker threads
    for (auto& thread : worker_threads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    
    // Wait for metrics thread
    if (metrics_thread_ && metrics_thread_->joinable()) {
        metrics_thread_->join();
    }
    
    // Wait for heartbeat thread
    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }
    
    // Cleanup LibWebSockets
    CleanupLibWebSockets();
    
    UpdateServerStatus(ServerStatus::STOPPED);
    LogInfo("WebSocket-Only Bridge stopped");
    return true;
}

bool WebSocketOnlyBridge::SendMessage(const std::string& client_id, const std::string& message, bool is_binary) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(client_id);
    if (it == connections_.end()) {
        LogWarning("Client not found: " + client_id);
        return false;
    }
    
    struct lws* wsi = it->second->wsi;
    if (!wsi) {
        LogError("Invalid WebSocket connection for client: " + client_id);
        return false;
    }
    
    // Prepare message with LWS_PRE padding
    size_t total_size = LWS_PRE + message.length();
    std::vector<uint8_t> send_buffer(total_size);
    std::memcpy(send_buffer.data() + LWS_PRE, message.data(), message.length());
    
    int protocol = is_binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT;
    int result = lws_write(wsi, send_buffer.data() + LWS_PRE, message.length(), 
                          static_cast<lws_write_protocol>(protocol));
    
    if (result < 0) {
        LogError("Failed to send message to client: " + client_id);
        metrics_.messages_failed++;
        return false;
    }
    
    // Update metrics
    it->second->messages_sent++;
    it->second->bytes_sent += message.length();
    metrics_.messages_sent++;
    metrics_.total_bytes_sent += message.length();
    
    LogDebug("Sent message to client " + client_id + ": " + message.substr(0, 100) + "...");
    return true;
}

bool WebSocketOnlyBridge::SendToRoom(const std::string& room, const std::string& message, bool is_binary) {
    std::lock_guard<std::mutex> rooms_lock(rooms_mutex_);
    
    auto room_it = rooms_.find(room);
    if (room_it == rooms_.end()) {
        LogWarning("Room not found: " + room);
        return false;
    }
    
    BroadcastToRoom(room, message, is_binary);
    UpdateRoomActivity(room);
    
    metrics_.messages_broadcast++;
    LogDebug("Broadcast message to room " + room + ": " + message.substr(0, 100) + "...");
    return true;
}

bool WebSocketOnlyBridge::BroadcastMessage(const std::string& message, bool is_binary) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    size_t sent_count = 0;
    for (const auto& pair : connections_) {
        if (SendMessage(pair.first, message, is_binary)) {
            sent_count++;
        }
    }
    
    metrics_.messages_broadcast++;
    LogDebug("Broadcast message to " + std::to_string(sent_count) + " clients: " + message.substr(0, 100) + "...");
    return sent_count > 0;
}

bool WebSocketOnlyBridge::SendSensorData(const std::string& room, const std::string& sensor_id, const Json::Value& data) {
    std::string message = WebSocketOnlyUtils::CreateSensorDataMessage(
        sensor_id,
        data.get("temperature", 0.0).asDouble(),
        data.get("humidity", 0.0).asDouble(),
        data.get("location", "").asString()
    );
    
    bool result = SendToRoom(room, message);
    
    if (result && config_.enable_thermal_processing) {
        ProcessThermalData(sensor_id, 
                         data.get("temperature", 0.0).asDouble(),
                         data.get("humidity", 0.0).asDouble());
    }
    
    return result;
}

void WebSocketOnlyBridge::ServerLoop() {
    LogInfo("WebSocket server loop started");
    
    while (running_.load()) {
        if (lws_context_) {
            int result = lws_service(lws_context_, 50);  // 50ms timeout
            
            if (result < 0) {
                LogError("LibWebSockets service error: " + std::to_string(result));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LogInfo("WebSocket server loop stopped");
}

void WebSocketOnlyBridge::WorkerLoop() {
    while (!stop_processing_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (message_queue_.empty()) {
            queue_cv_.wait_for(lock, std::chrono::milliseconds(100));
            continue;
        }
        
        InternalMessage msg = message_queue_.front();
        message_queue_.pop();
        metrics_.current_queue_size = message_queue_.size();
        
        lock.unlock();
        
        auto start_time = std::chrono::steady_clock::now();
        ProcessMessage(msg);
        auto end_time = std::chrono::steady_clock::now();
        
        double processing_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        UpdateProcessingMetrics(processing_time);
    }
}

void WebSocketOnlyBridge::ProcessMessage(const InternalMessage& msg) {
    switch (msg.type) {
        case MessageType::SENSOR_DATA:
            ProcessSensorData(msg.from_client, msg.room, msg.payload);
            break;
        case MessageType::ALERT:
            ProcessAlertMessage(msg.from_client, msg.room, msg.payload);
            break;
        case MessageType::COMMAND:
            ProcessCommand(msg.from_client, msg.to_client, msg.payload);
            break;
        case MessageType::ROOM_MESSAGE:
            ProcessRoomMessage(msg.from_client, msg.room, msg.payload);
            break;
        case MessageType::PRIVATE_MESSAGE:
            ProcessPrivateMessage(msg.from_client, msg.to_client, msg.payload);
            break;
        case MessageType::ROOM_JOIN:
            JoinRoom(msg.from_client, msg.room);
            break;
        case MessageType::ROOM_LEAVE:
            LeaveRoom(msg.from_client, msg.room);
            break;
        default:
            LogDebug("Unknown message type: " + std::to_string(static_cast<int>(msg.type)));
    }
}

void WebSocketOnlyBridge::ProcessSensorData(const std::string& from_client, const std::string& room, const std::string& payload) {
    std::string sensor_id, location;
    double temperature, humidity;
    
    if (WebSocketOnlyUtils::ParseSensorDataMessage(payload, sensor_id, temperature, humidity, location)) {
        LogDebug("Processed sensor data from " + from_client + " - ID: " + sensor_id + 
                ", Temp: " + std::to_string(temperature) + "°C");
        
        if (config_.enable_thermal_processing) {
            ProcessThermalData(sensor_id, temperature, humidity);
        }
        
        // Broadcast to room
        SendToRoom(room, payload);
        
        if (message_callback_) {
            message_callback_(from_client, payload, room);
        }
    } else {
        LogError("Failed to parse sensor data payload from " + from_client);
    }
}

bool WebSocketOnlyBridge::ProcessThermalData(const std::string& sensor_id, double temperature, double humidity) {
    if (!thermal_enabled_.load() || !thermal_tracker_) return false;
    
    std::lock_guard<std::mutex> lock(thermal_mutex_);
    
    bool result = thermal_tracker_->process_sensor_data(sensor_id, temperature, humidity, "");
    last_sensor_update_[sensor_id] = std::chrono::steady_clock::now();
    
    return result;
}

// Room management
bool WebSocketOnlyBridge::CreateRoom(const std::string& room_name, const std::string& description, bool is_private) {
    if (room_name.empty() || !WebSocketOnlyUtils::ValidateRoomName(room_name)) {
        LogError("Invalid room name: " + room_name);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(rooms_mutex_);
    
    if (rooms_.find(room_name) != rooms_.end()) {
        LogWarning("Room already exists: " + room_name);
        return false;
    }
    
    if (rooms_.size() >= config_.max_rooms) {
        LogError("Maximum number of rooms reached: " + std::to_string(config_.max_rooms));
        return false;
    }
    
    auto room = std::make_unique<RoomInfo>(room_name);
    room->description = description;
    room->is_private = is_private;
    room->max_clients = config_.max_room_size;
    
    rooms_[room_name] = std::move(room);
    metrics_.rooms_created++;
    
    LogInfo("Created room: " + room_name + (is_private ? " (private)" : " (public)"));
    
    if (room_callback_) {
        room_callback_(room_name, "", true);  // Empty client_id for room creation
    }
    
    return true;
}

bool WebSocketOnlyBridge::JoinRoom(const std::string& client_id, const std::string& room_name) {
    std::lock_guard<std::mutex> rooms_lock(rooms_mutex_);
    std::lock_guard<std::mutex> conn_lock(connections_mutex_);
    
    // Check if client exists
    auto client_it = connections_.find(client_id);
    if (client_it == connections_.end()) {
        LogError("Client not found: " + client_id);
        return false;
    }
    
    // Get or create room
    auto room_it = rooms_.find(room_name);
    if (room_it == rooms_.end()) {
        if (config_.auto_create_rooms) {
            auto room = std::make_unique<RoomInfo>(room_name);
            room->description = "Auto-created room";
            rooms_[room_name] = std::move(room);
            room_it = rooms_.find(room_name);
            metrics_.rooms_created++;
            LogInfo("Auto-created room: " + room_name);
        } else {
            LogError("Room not found: " + room_name);
            return false;
        }
    }
    
    // Check room capacity
    if (room_it->second->clients.size() >= room_it->second->max_clients) {
        LogError("Room is full: " + room_name);
        return false;
    }
    
    // Remove client from current room
    std::string old_room = client_it->second->room;
    if (!old_room.empty() && old_room != room_name) {
        auto old_room_it = rooms_.find(old_room);
        if (old_room_it != rooms_.end()) {
            old_room_it->second->clients.erase(client_id);
        }
    }
    
    // Add client to new room
    room_it->second->clients.insert(client_id);
    client_it->second->room = room_name;
    
    LogInfo("Client " + client_id + " joined room: " + room_name);
    
    // Notify room
    std::string join_message = WebSocketOnlyUtils::CreateRoomMessage("join", room_name, client_id + " joined the room");
    BroadcastToRoom(room_name, join_message, false, client_id);
    
    if (room_callback_) {
        room_callback_(room_name, client_id, true);
    }
    
    return true;
}

// Static LibWebSockets callbacks
int WebSocketOnlyBridge::WebSocketCallback(struct lws* wsi, enum lws_callback_reasons reason,
                                          void* user, void* in, size_t len) {
    WebSocketOnlyBridge* bridge = g_ws_bridge_instance;
    if (!bridge) return 0;
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            bridge->HandleNewConnection(wsi);
            break;
            
        case LWS_CALLBACK_CLOSED:
            bridge->HandleConnectionClose(wsi);
            break;
            
        case LWS_CALLBACK_RECEIVE:
            bridge->HandleIncomingMessage(wsi, static_cast<const uint8_t*>(in), len);
            break;
            
        case LWS_CALLBACK_SERVER_WRITEABLE:
            // Handle writable events if needed
            break;
            
        default:
            break;
    }
    
    return 0;
}

int WebSocketOnlyBridge::HttpCallback(struct lws* wsi, enum lws_callback_reasons reason,
                                     void* user, void* in, size_t len) {
    // Handle HTTP requests if needed
    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

void WebSocketOnlyBridge::HandleNewConnection(struct lws* wsi) {
    std::string client_id = GenerateClientId();
    
    auto connection = std::make_unique<ConnectionInfo>();
    connection->wsi = wsi;
    connection->client_id = client_id;
    connection->room = config_.default_room;
    
    // Get client info
    char name[256], rip[256];
    lws_get_peer_simple(wsi, name, sizeof(name));
    lws_get_peer_simple(wsi, rip, sizeof(rip));
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_[client_id] = std::move(connection);
    wsi_to_client_[wsi] = client_id;
    
    // Update metrics
    metrics_.connections_total++;
    metrics_.connections_current++;
    if (metrics_.connections_current > metrics_.connections_peak) {
        metrics_.connections_peak = metrics_.connections_current.load();
    }
    
    // Join default room
    if (!config_.default_room.empty()) {
        JoinRoom(client_id, config_.default_room);
    }
    
    LogInfo("New WebSocket connection: " + client_id + " from " + std::string(rip));
    
    if (connection_callback_) {
        connection_callback_(client_id, true);
    }
}

void WebSocketOnlyBridge::HandleConnectionClose(struct lws* wsi) {
    std::lock_guard<std::mutex> conn_lock(connections_mutex_);
    
    auto wsi_it = wsi_to_client_.find(wsi);
    if (wsi_it == wsi_to_client_.end()) {
        LogWarning("Unknown WebSocket connection closed");
        return;
    }
    
    std::string client_id = wsi_it->second;
    auto conn_it = connections_.find(client_id);
    if (conn_it != connections_.end()) {
        std::string room = conn_it->second->room;
        
        // Remove from room
        if (!room.empty()) {
            std::lock_guard<std::mutex> rooms_lock(rooms_mutex_);
            auto room_it = rooms_.find(room);
            if (room_it != rooms_.end()) {
                room_it->second->clients.erase(client_id);
                
                // Notify room about departure
                std::string leave_message = WebSocketOnlyUtils::CreateRoomMessage("leave", room, client_id + " left the room");
                BroadcastToRoom(room, leave_message, false, client_id);
            }
        }
        
        connections_.erase(conn_it);
    }
    
    wsi_to_client_.erase(wsi_it);
    metrics_.connections_current--;
    
    LogInfo("WebSocket connection closed: " + client_id);
    
    if (connection_callback_) {
        connection_callback_(client_id, false);
    }
}

void WebSocketOnlyBridge::HandleIncomingMessage(struct lws* wsi, const uint8_t* data, size_t len) {
    auto wsi_it = wsi_to_client_.find(wsi);
    if (wsi_it == wsi_to_client_.end()) {
        LogWarning("Message from unknown WebSocket connection");
        return;
    }
    
    std::string client_id = wsi_it->second;
    std::string message(reinterpret_cast<const char*>(data), len);
    
    // Update client activity
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto conn_it = connections_.find(client_id);
        if (conn_it != connections_.end()) {
            conn_it->second->last_activity = std::chrono::steady_clock::now();
            conn_it->second->messages_received++;
            conn_it->second->bytes_received += len;
        }
    }
    
    // Create internal message for processing
    InternalMessage internal_msg;
    internal_msg.type = DetermineMessageType(message);
    internal_msg.from_client = client_id;
    internal_msg.payload = message;
    internal_msg.timestamp = std::chrono::steady_clock::now();
    
    // Determine target room
    std::lock_guard<std::mutex> conn_lock(connections_mutex_);
    auto conn_it = connections_.find(client_id);
    if (conn_it != connections_.end()) {
        internal_msg.room = conn_it->second->room;
    }
    
    // Add to processing queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push(internal_msg);
        metrics_.current_queue_size = message_queue_.size();
        
        if (metrics_.current_queue_size > metrics_.max_queue_size) {
            metrics_.max_queue_size = metrics_.current_queue_size.load();
        }
    }
    
    queue_cv_.notify_one();
    metrics_.messages_received++;
    metrics_.total_bytes_received += len;
    
    LogDebug("Received message from " + client_id + ": " + message.substr(0, 100) + "...");
}

//=============================================================================
// WebSocketOnlyUtils Implementation
//=============================================================================

namespace WebSocketOnlyUtils {

std::string CreateSensorDataMessage(const std::string& sensor_id, double temperature, 
                                   double humidity, const std::string& location) {
    Json::Value json;
    json["type"] = "sensor_data";
    json["sensor_id"] = sensor_id;
    json["temperature"] = temperature;
    json["humidity"] = humidity;
    json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (!location.empty()) {
        json["location"] = location;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, json);
}

std::string CreateAlertMessage(const std::string& sensor_id, const std::string& alert_type,
                              const std::string& message, double value) {
    Json::Value json;
    json["type"] = "alert";
    json["sensor_id"] = sensor_id;
    json["alert_type"] = alert_type;
    json["message"] = message;
    json["value"] = value;
    json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, json);
}

std::string CreateRoomMessage(const std::string& action, const std::string& room,
                             const std::string& message) {
    Json::Value json;
    json["type"] = "room";
    json["action"] = action;
    json["room"] = room;
    json["message"] = message;
    json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, json);
}

bool ParseSensorDataMessage(const std::string& message, std::string& sensor_id, 
                           double& temperature, double& humidity, std::string& location) {
    try {
        Json::Value json;
        Json::CharReaderBuilder reader_builder;
        std::string errors;
        std::stringstream ss(message);
        
        if (!Json::parseFromStream(reader_builder, ss, &json, &errors)) {
            return false;
        }
        
        if (!json.isMember("sensor_id") || !json.isMember("temperature") || !json.isMember("humidity")) {
            return false;
        }
        
        sensor_id = json["sensor_id"].asString();
        temperature = json["temperature"].asDouble();
        humidity = json["humidity"].asDouble();
        location = json.get("location", "").asString();
        
        return true;
    } catch (const std::exception& /*e*/) {
        return false;
    }
}

double CalculateThroughput(uint64_t message_count, std::chrono::milliseconds duration) {
    if (duration.count() == 0) return 0.0;
    return (static_cast<double>(message_count) * 1000.0) / duration.count();
}

std::string FormatMetrics(const WebSocketOnlyBridge::Metrics& metrics) {
    std::stringstream ss;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.start_time);
    
    ss << "=== WebSocket-Only Bridge Metrics ===\n";
    ss << "Uptime: " << duration.count() << " seconds\n";
    ss << "Current Connections: " << metrics.connections_current.load() << "\n";
    ss << "Peak Connections: " << metrics.connections_peak.load() << "\n";
    ss << "Total Connections: " << metrics.connections_total.load() << "\n";
    ss << "Messages Received: " << metrics.messages_received.load() << "\n";
    ss << "Messages Sent: " << metrics.messages_sent.load() << "\n";
    ss << "Messages Broadcast: " << metrics.messages_broadcast.load() << "\n";
    ss << "Messages Failed: " << metrics.messages_failed.load() << "\n";
    ss << "Active Rooms: " << metrics.rooms_active.load() << "\n";
    ss << "Total Rooms Created: " << metrics.rooms_created.load() << "\n";
    ss << "Current Queue Size: " << metrics.current_queue_size.load() << "\n";
    ss << "Max Queue Size: " << metrics.max_queue_size.load() << "\n";
    ss << "Avg Processing Time: " << std::fixed << std::setprecision(2) 
       << metrics.avg_processing_time_ms.load() << " ms\n";
    ss << "Max Processing Time: " << std::fixed << std::setprecision(2) 
       << metrics.max_processing_time_ms.load() << " ms\n";
    ss << "Throughput: " << std::fixed << std::setprecision(1) 
       << CalculateThroughput(metrics.messages_received.load(), 
                            std::chrono::duration_cast<std::chrono::milliseconds>(duration))
       << " msg/sec\n";
    
    return ss.str();
}

WebSocketOnlyBridge::Config CreateDefaultConfig() {
    return WebSocketOnlyBridge::Config();
}

WebSocketOnlyBridge::Config CreateHighThroughputConfig() {
    WebSocketOnlyBridge::Config config;
    config.port = 8080;
    config.max_connections = 2000;
    config.worker_thread_count = 8;
    config.message_buffer_size = 20000;
    config.max_frame_size = 131072;  // 128KB
    config.enable_compression = true;
    config.enable_per_message_deflate = true;
    return config;
}

WebSocketOnlyBridge::Config CreateLowLatencyConfig() {
    WebSocketOnlyBridge::Config config;
    config.port = 8080;
    config.max_connections = 500;
    config.worker_thread_count = 2;
    config.message_buffer_size = 1000;
    config.max_frame_size = 4096;
    config.ping_interval_seconds = 15;
    config.enable_compression = false;  // Disable compression for speed
    config.enable_per_message_deflate = false;
    return config;
}

bool ValidateRoomName(const std::string& room_name) {
    if (room_name.empty() || room_name.length() > 50) return false;
    // Check for valid characters (alphanumeric, underscore, hyphen)
    return std::all_of(room_name.begin(), room_name.end(), [](char c) {
        return std::isalnum(c) || c == '_' || c == '-';
    });
}

bool ValidateClientId(const std::string& client_id) {
    return !client_id.empty() && client_id.length() <= 100;
}

std::string ServerStatusToString(WebSocketOnlyBridge::ServerStatus status) {
    switch (status) {
        case WebSocketOnlyBridge::ServerStatus::STOPPED: return "STOPPED";
        case WebSocketOnlyBridge::ServerStatus::STARTING: return "STARTING";
        case WebSocketOnlyBridge::ServerStatus::RUNNING: return "RUNNING";
        case WebSocketOnlyBridge::ServerStatus::STOPPING: return "STOPPING";
        case WebSocketOnlyBridge::ServerStatus::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void RunPerformanceTest(WebSocketOnlyBridge& bridge, int duration_seconds, int messages_per_second) {
    std::cout << "Starting WebSocket performance test..." << std::endl;
    std::cout << "Duration: " << duration_seconds << " seconds" << std::endl;
    std::cout << "Target rate: " << messages_per_second << " msg/sec" << std::endl;
    
    auto start_metrics = bridge.GetMetrics();
    auto start_time = std::chrono::steady_clock::now();
    
    int message_interval_us = 1000000 / messages_per_second;
    int total_messages = duration_seconds * messages_per_second;
    
    for (int i = 0; i < total_messages && bridge.IsRunning(); ++i) {
        std::string message = CreateSensorDataMessage(
            "test_sensor_" + std::to_string(i % 10),
            20.0 + (rand() % 100) / 10.0,
            40.0 + (rand() % 400) / 10.0,
            "test_room"
        );
        
        bridge.BroadcastMessage(message);
        std::this_thread::sleep_for(std::chrono::microseconds(message_interval_us));
    }
    
    // Wait for processing to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auto end_time = std::chrono::steady_clock::now();
    auto end_metrics = bridge.GetMetrics();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    uint64_t messages_sent = end_metrics.messages_sent - start_metrics.messages_sent;
    uint64_t messages_received = end_metrics.messages_received - start_metrics.messages_received;
    
    std::cout << "\n=== WebSocket Performance Test Results ===" << std::endl;
    std::cout << "Actual duration: " << duration.count() << " ms" << std::endl;
    std::cout << "Messages sent: " << messages_sent << std::endl;
    std::cout << "Messages received: " << messages_received << std::endl;
    std::cout << "Actual throughput: " << CalculateThroughput(messages_sent, duration) << " msg/sec" << std::endl;
    std::cout << "Success rate: " << (100.0 * messages_sent / total_messages) << "%" << std::endl;
}

} // namespace WebSocketOnlyUtils

// Additional WebSocketOnlyBridge methods

WebSocketOnlyBridge::MessageType WebSocketOnlyBridge::DetermineMessageType(const std::string& message) {
    try {
        Json::Value json;
        Json::CharReaderBuilder reader_builder;
        std::string errors;
        std::stringstream ss(message);
        
        if (Json::parseFromStream(reader_builder, ss, &json, &errors)) {
            std::string type = json.get("type", "").asString();
            
            if (type == "sensor_data") return MessageType::SENSOR_DATA;
            if (type == "alert") return MessageType::ALERT;
            if (type == "command") return MessageType::COMMAND;
            if (type == "status") return MessageType::STATUS;
            if (type == "room") return MessageType::ROOM_MESSAGE;
            if (type == "private") return MessageType::PRIVATE_MESSAGE;
            if (type == "heartbeat") return MessageType::HEARTBEAT;
        }
    } catch (const std::exception& /*e*/) {
        // Fall through to default
    }
    
    return MessageType::SYSTEM_MESSAGE;
}

std::string WebSocketOnlyBridge::GenerateClientId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    return "ws_client_" + std::to_string(dis(gen));
}

bool WebSocketOnlyBridge::InitializeLibWebSockets() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    // Define protocols
    static struct lws_protocols protocols[] = {
        {
            "http-only",
            HttpCallback,
            0,
            0,
        },
        {
            "websocket-protocol",
            WebSocketCallback,
            sizeof(PerSessionData),
            static_cast<size_t>(config_.max_frame_size),
        },
        { nullptr, nullptr, 0, 0 } // terminator
    };
    
    info.port = config_.port;
    info.iface = config_.host.c_str();
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;
    
    if (config_.enable_compression) {
        info.options |= LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
    }
    
    lws_context_ = lws_create_context(&info);
    if (!lws_context_) {
        LogError("Failed to create LibWebSockets context");
        return false;
    }
    
    LogInfo("LibWebSockets initialized on " + config_.host + ":" + std::to_string(config_.port));
    return true;
}

void WebSocketOnlyBridge::CleanupLibWebSockets() {
    if (lws_context_) {
        lws_context_destroy(lws_context_);
        lws_context_ = nullptr;
    }
}

WebSocketOnlyBridge::Metrics WebSocketOnlyBridge::GetMetrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

bool WebSocketOnlyBridge::IsRunning() const {
    return running_.load();
}

double WebSocketOnlyBridge::GetCurrentThroughput() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics_.start_time);
    return WebSocketOnlyUtils::CalculateThroughput(metrics_.messages_received.load(), duration);
}

bool WebSocketOnlyBridge::EnableThermalMonitoring(const ThermalConfig& thermal_config) {
    if (thermal_enabled_.load()) {
        LogWarning("Thermal monitoring already enabled");
        return true;
    }
    
    try {
        thermal_tracker_ = std::make_unique<ThermalIsolationTracker>(thermal_config);
        
        // Set alert callback
        thermal_tracker_->set_alert_callback([this](const Alert& alert) {
            if (alert_callback_) {
                alert_callback_(alert);
            }
            
            // Broadcast alert to all rooms
            std::string alert_message = WebSocketOnlyUtils::CreateAlertMessage(
                alert.sensor_id, "thermal_alert", "", alert.temperature);
            
            BroadcastMessage(alert_message);
            metrics_.thermal_alerts_generated++;
        });
        
        if (thermal_tracker_->start()) {
            thermal_enabled_ = true;
            LogInfo("Thermal monitoring enabled");
            return true;
        } else {
            LogError("Failed to start thermal monitoring");
            thermal_tracker_.reset();
            return false;
        }
    } catch (const std::exception& e) {
        LogError("Exception enabling thermal monitoring: " + std::string(e.what()));
        return false;
    }
}

void WebSocketOnlyBridge::DisableThermalMonitoring() {
    if (!thermal_enabled_.load()) return;
    
    thermal_enabled_ = false;
    if (thermal_tracker_) {
        thermal_tracker_->stop();
        thermal_tracker_.reset();
    }
    LogInfo("Thermal monitoring disabled");
}

void WebSocketOnlyBridge::UpdateServerStatus(ServerStatus status, const std::string& message) {
    server_status_ = status;
    if (status_callback_) {
        status_callback_(status, message);
    }
}

void WebSocketOnlyBridge::UpdateProcessingMetrics(double processing_time_ms) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // Update average processing time (simple moving average)
    double current_avg = metrics_.avg_processing_time_ms.load();
    double new_avg = (current_avg * 0.9) + (processing_time_ms * 0.1);
    metrics_.avg_processing_time_ms = new_avg;
    
    // Update max processing time
    if (processing_time_ms > metrics_.max_processing_time_ms.load()) {
        metrics_.max_processing_time_ms = processing_time_ms;
    }
}

void WebSocketOnlyBridge::BroadcastToRoom(const std::string& room, const std::string& message, bool is_binary, const std::string& exclude_client) {
    auto room_it = rooms_.find(room);
    if (room_it == rooms_.end()) return;
    
    for (const std::string& client_id : room_it->second->clients) {
        if (client_id != exclude_client) {
            SendMessage(client_id, message, is_binary);
        }
    }
    
    room_it->second->message_count++;
    room_it->second->total_bytes += message.length();
}

void WebSocketOnlyBridge::UpdateRoomActivity(const std::string& room) {
    // Update room activity timestamp if needed
}

void WebSocketOnlyBridge::MetricsLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        if (running_.load()) {
            // Update active rooms count
            std::lock_guard<std::mutex> lock(rooms_mutex_);
            size_t active_count = 0;
            for (const auto& pair : rooms_) {
                if (!pair.second->clients.empty()) {
                    active_count++;
                }
            }
            metrics_.rooms_active = active_count;
            
            LogDebug("Metrics: " + std::to_string(metrics_.connections_current.load()) + 
                    " connections, " + std::to_string(metrics_.messages_received.load()) + " messages received");
        }
    }
}

void WebSocketOnlyBridge::HeartbeatLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(config_.ping_interval_seconds));
        
        if (running_.load()) {
            // Send heartbeat to all connected clients
            std::string heartbeat = WebSocketOnlyUtils::CreateRoomMessage("heartbeat", "", "ping");
            BroadcastMessage(heartbeat);
        }
    }
}

// Stub implementations for missing methods
void WebSocketOnlyBridge::LogInfo(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

void WebSocketOnlyBridge::LogError(const std::string& message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

void WebSocketOnlyBridge::LogDebug(const std::string& message) {
    std::cout << "[DEBUG] " << message << std::endl;
}

void WebSocketOnlyBridge::LogWarning(const std::string& message) {
    std::cout << "[WARNING] " << message << std::endl;
}

// Additional stub methods to satisfy the interface
bool WebSocketOnlyBridge::Restart() { return Stop() && Start(); }
WebSocketOnlyBridge::Config WebSocketOnlyBridge::GetConfig() const { return config_; }
std::vector<std::string> WebSocketOnlyBridge::GetConnectedClients() const { 
    std::lock_guard<std::mutex> lock(connections_mutex_);
    std::vector<std::string> clients;
    for (const auto& pair : connections_) {
        clients.push_back(pair.first);
    }
    return clients;
}

WebSocketOnlyBridge::ConnectionInfo WebSocketOnlyBridge::GetConnectionInfo(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(client_id);
    if (it != connections_.end()) {
        return *it->second;
    }
    return ConnectionInfo();
}

size_t WebSocketOnlyBridge::GetConnectionCount() const { return metrics_.connections_current.load(); }
WebSocketOnlyBridge::ServerStatus WebSocketOnlyBridge::GetServerStatus() const { return server_status_.load(); }
void WebSocketOnlyBridge::ResetMetrics() { /* Reset implementation */ }
double WebSocketOnlyBridge::GetAverageLatency() const { return metrics_.avg_processing_time_ms.load(); }
size_t WebSocketOnlyBridge::GetQueueSize() const { return metrics_.current_queue_size.load(); }
std::string WebSocketOnlyBridge::GetServerInfo() const { return "WebSocket-Only Bridge on " + config_.host + ":" + std::to_string(config_.port); }

// Callback setters
void WebSocketOnlyBridge::SetMessageCallback(MessageCallback callback) { message_callback_ = callback; }
void WebSocketOnlyBridge::SetConnectionCallback(ConnectionCallback callback) { connection_callback_ = callback; }
void WebSocketOnlyBridge::SetAlertCallback(AlertCallback callback) { alert_callback_ = callback; }
void WebSocketOnlyBridge::SetRoomCallback(RoomCallback callback) { room_callback_ = callback; }
void WebSocketOnlyBridge::SetStatusCallback(StatusCallback callback) { status_callback_ = callback; }

