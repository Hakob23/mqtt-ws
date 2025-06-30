#include "mqtt_only_bridge.h"
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

using namespace thermal_monitoring;

//=============================================================================
// MQTTOnlyBridge Implementation
//=============================================================================

MQTTOnlyBridge::MQTTOnlyBridge(const Config& config) 
    : config_(config), mosq_(nullptr) {
    metrics_.start_time = std::chrono::steady_clock::now();
    mosquitto_lib_init();
}

MQTTOnlyBridge::~MQTTOnlyBridge() {
    Stop();
    CleanupMosquitto();
    mosquitto_lib_cleanup();
}

bool MQTTOnlyBridge::Start() {
    if (running_.load()) {
        LogWarning("Bridge already running");
        return true;
    }
    
    LogInfo("Starting MQTT-Only Bridge...");
    
    if (!InitializeMosquitto()) {
        LogError("Failed to initialize Mosquitto");
        return false;
    }
    
    if (!ConnectToBroker()) {
        LogError("Failed to connect to MQTT broker");
        return false;
    }
    
    // Start worker threads
    stop_processing_ = false;
    for (int i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.emplace_back(std::make_unique<std::thread>(&MQTTOnlyBridge::WorkerLoop, this));
    }
    
    // Start metrics thread
    metrics_thread_ = std::make_unique<std::thread>(&MQTTOnlyBridge::MetricsLoop, this);
    
    // Start heartbeat thread
    heartbeat_thread_ = std::make_unique<std::thread>(&MQTTOnlyBridge::HeartbeatLoop, this);
    
    // Start main thread
    main_thread_ = std::make_unique<std::thread>(&MQTTOnlyBridge::MainLoop, this);
    
    running_ = true;
    LogInfo("MQTT-Only Bridge started successfully");
    return true;
}

bool MQTTOnlyBridge::Stop() {
    if (!running_.load()) {
        return true;
    }
    
    LogInfo("Stopping MQTT-Only Bridge...");
    running_ = false;
    stop_processing_ = true;
    
    // Notify all threads to stop
    queue_cv_.notify_all();
    
    // Wait for main thread
    if (main_thread_ && main_thread_->joinable()) {
        main_thread_->join();
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
    
    // Disconnect from broker
    if (mosq_) {
        mosquitto_disconnect(mosq_);
    }
    
    UpdateConnectionStatus(ConnectionStatus::DISCONNECTED);
    LogInfo("MQTT-Only Bridge stopped");
    return true;
}

bool MQTTOnlyBridge::PublishSensorData(const std::string& sensor_id, const Json::Value& data) {
    if (!IsConnected()) return false;
    
    std::string topic = MQTTOnlyUtils::BuildSensorTopic(config_.sensor_topic_prefix, sensor_id);
    std::string payload = MQTTOnlyUtils::CreateSensorDataPayload(
        sensor_id, 
        data.get("temperature", 0.0).asDouble(),
        data.get("humidity", 0.0).asDouble(),
        data.get("location", "").asString()
    );
    
    bool result = PublishMessage(topic, payload, config_.sensor_data_qos);
    
    if (result && config_.enable_thermal_processing) {
        ProcessThermalData(sensor_id, 
                         data.get("temperature", 0.0).asDouble(),
                         data.get("humidity", 0.0).asDouble());
    }
    
    return result;
}

bool MQTTOnlyBridge::PublishMessage(const std::string& topic, const std::string& payload, int qos, bool retain) {
    if (!mosq_ || !IsConnected()) return false;
    
    if (qos == -1) qos = config_.default_qos;
    
    // Check topic filtering
    if (!IsTopicAllowed(topic)) {
        LogWarning("Topic blocked by filter: " + topic);
        return false;
    }
    
    int mid;
    int result = mosquitto_publish(mosq_, &mid, topic.c_str(), payload.length(), payload.c_str(), qos, retain);
    
    if (result == MOSQ_ERR_SUCCESS) {
        metrics_.messages_published++;
        metrics_.total_bytes_sent += payload.length();
        LogDebug("Published to " + topic + ": " + payload);
        return true;
    } else {
        metrics_.messages_failed++;
        LogError("Failed to publish: " + std::string(mosquitto_strerror(result)));
        return false;
    }
}

void MQTTOnlyBridge::MainLoop() {
    LogInfo("Main loop started");
    
    while (running_.load()) {
        int result = mosquitto_loop(mosq_, 100, 1);
        
        if (result != MOSQ_ERR_SUCCESS) {
            if (result == MOSQ_ERR_CONN_LOST) {
                LogWarning("Connection lost, attempting reconnection...");
                HandleReconnection();
            } else {
                LogError("Mosquitto loop error: " + std::string(mosquitto_strerror(result)));
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay));
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    LogInfo("Main loop stopped");
}

void MQTTOnlyBridge::WorkerLoop() {
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
        
        metrics_.messages_processed++;
    }
}

void MQTTOnlyBridge::ProcessMessage(const InternalMessage& msg) {
    switch (msg.type) {
        case MessageType::SENSOR_DATA:
            ProcessSensorData(msg.topic, msg.payload);
            break;
        case MessageType::ALERT:
            ProcessAlertMessage(msg.topic, msg.payload);
            break;
        case MessageType::COMMAND:
            ProcessCommand(msg.topic, msg.payload);
            break;
        case MessageType::STATUS:
            ProcessStatusMessage(msg.topic, msg.payload);
            break;
        case MessageType::RESPONSE:
            ProcessResponse(msg.topic, msg.payload);
            break;
        case MessageType::HEARTBEAT:
            ProcessHeartbeat(msg.topic, msg.payload);
            break;
        default:
            LogDebug("Unknown message type: " + std::to_string(static_cast<int>(msg.type)));
    }
}

void MQTTOnlyBridge::ProcessSensorData(const std::string& topic, const std::string& payload) {
    std::string sensor_id, location;
    double temperature, humidity;
    
    if (MQTTOnlyUtils::ParseSensorDataPayload(payload, sensor_id, temperature, humidity, location)) {
        LogDebug("Processed sensor data - ID: " + sensor_id + ", Temp: " + std::to_string(temperature) + "°C");
        
        if (config_.enable_thermal_processing) {
            ProcessThermalData(sensor_id, temperature, humidity);
        }
        
        if (message_callback_) {
            message_callback_(topic, payload, 0);
        }
    } else {
        LogError("Failed to parse sensor data payload");
    }
}

bool MQTTOnlyBridge::ProcessThermalData(const std::string& sensor_id, double temperature, double humidity) {
    if (!thermal_enabled_.load() || !thermal_tracker_) return false;
    
    std::lock_guard<std::mutex> lock(thermal_mutex_);
    
    bool result = thermal_tracker_->process_sensor_data(sensor_id, temperature, humidity, "");
    last_sensor_update_[sensor_id] = std::chrono::steady_clock::now();
    
    return result;
}

// Static MQTT callbacks
void MQTTOnlyBridge::OnConnect(struct mosquitto* mosq, void* userdata, int result) {
    MQTTOnlyBridge* bridge = static_cast<MQTTOnlyBridge*>(userdata);
    
    if (result == 0) {
        bridge->LogInfo("Connected to MQTT broker");
        bridge->UpdateConnectionStatus(ConnectionStatus::CONNECTED);
        bridge->metrics_.connection_count++;
        bridge->metrics_.is_connected = true;
    } else {
        bridge->LogError("Failed to connect to MQTT broker: " + std::string(mosquitto_connack_string(result)));
        bridge->UpdateConnectionStatus(ConnectionStatus::ERROR);
    }
}

void MQTTOnlyBridge::OnDisconnect(struct mosquitto* mosq, void* userdata, int result) {
    MQTTOnlyBridge* bridge = static_cast<MQTTOnlyBridge*>(userdata);
    
    bridge->LogWarning("Disconnected from MQTT broker");
    bridge->UpdateConnectionStatus(ConnectionStatus::DISCONNECTED);
    bridge->metrics_.is_connected = false;
    
    if (result != 0) {
        bridge->LogInfo("Unexpected disconnection, will attempt to reconnect");
    }
}

void MQTTOnlyBridge::OnMessage(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* msg) {
    MQTTOnlyBridge* bridge = static_cast<MQTTOnlyBridge*>(userdata);
    
    if (!msg || !msg->payload) return;
    
    std::string topic(msg->topic);
    std::string payload(static_cast<const char*>(msg->payload), msg->payloadlen);
    
    // Create internal message
    InternalMessage internal_msg;
    internal_msg.type = bridge->DetermineMessageType(topic);
    internal_msg.topic = topic;
    internal_msg.payload = payload;
    internal_msg.qos = msg->qos;
    internal_msg.timestamp = std::chrono::steady_clock::now();
    internal_msg.retain = msg->retain;
    
    // Add to processing queue
    {
        std::lock_guard<std::mutex> lock(bridge->queue_mutex_);
        bridge->message_queue_.push(internal_msg);
        bridge->metrics_.current_queue_size = bridge->message_queue_.size();
        
        if (bridge->metrics_.current_queue_size > bridge->metrics_.max_queue_size) {
            bridge->metrics_.max_queue_size = bridge->metrics_.current_queue_size;
        }
    }
    
    bridge->queue_cv_.notify_one();
    bridge->metrics_.messages_received++;
    bridge->metrics_.total_bytes_received += msg->payloadlen;
}

MQTTOnlyBridge::MessageType MQTTOnlyBridge::DetermineMessageType(const std::string& topic) {
    if (topic.find(config_.sensor_topic_prefix) == 0) {
        return MessageType::SENSOR_DATA;
    } else if (topic.find(config_.alert_topic_prefix) == 0) {
        return MessageType::ALERT;
    } else if (topic.find(config_.command_topic_prefix) == 0) {
        return MessageType::COMMAND;
    } else if (topic.find(config_.status_topic_prefix) == 0) {
        return MessageType::STATUS;
    } else if (topic.find(config_.response_topic_prefix) == 0) {
        return MessageType::RESPONSE;
    } else if (topic.find("heartbeat") != std::string::npos) {
        return MessageType::HEARTBEAT;
    }
    return MessageType::SYSTEM_MESSAGE;
}

bool MQTTOnlyBridge::InitializeMosquitto() {
    mosq_ = mosquitto_new(config_.client_id.c_str(), config_.clean_session, this);
    if (!mosq_) {
        LogError("Failed to create mosquitto instance");
        return false;
    }
    
    // Set callbacks
    mosquitto_connect_callback_set(mosq_, OnConnect);
    mosquitto_disconnect_callback_set(mosq_, OnDisconnect);
    mosquitto_message_callback_set(mosq_, OnMessage);
    mosquitto_publish_callback_set(mosq_, OnPublish);
    mosquitto_subscribe_callback_set(mosq_, OnSubscribe);
    mosquitto_log_callback_set(mosq_, OnLog);
    
    // Set authentication if provided
    if (!config_.username.empty()) {
        int result = mosquitto_username_pw_set(mosq_, config_.username.c_str(), 
                                              config_.password.empty() ? nullptr : config_.password.c_str());
        if (result != MOSQ_ERR_SUCCESS) {
            LogError("Failed to set username/password");
            return false;
        }
    }
    
    // Set TLS if enabled
    if (config_.use_tls) {
        int result = mosquitto_tls_set(mosq_, 
                                      config_.ca_cert_path.empty() ? nullptr : config_.ca_cert_path.c_str(),
                                      nullptr,
                                      config_.client_cert_path.empty() ? nullptr : config_.client_cert_path.c_str(),
                                      config_.client_key_path.empty() ? nullptr : config_.client_key_path.c_str(),
                                      nullptr);
        if (result != MOSQ_ERR_SUCCESS) {
            LogError("Failed to set TLS configuration");
            return false;
        }
    }
    
    // Set options
    mosquitto_max_inflight_messages_set(mosq_, config_.max_inflight_messages);
    mosquitto_message_retry_set(mosq_, config_.message_retry_count);
    
    return true;
}

bool MQTTOnlyBridge::ConnectToBroker() {
    if (!mosq_) return false;
    
    UpdateConnectionStatus(ConnectionStatus::CONNECTING);
    
    int result = mosquitto_connect(mosq_, config_.broker_host.c_str(), config_.broker_port, config_.keepalive);
    
    if (result == MOSQ_ERR_SUCCESS) {
        LogInfo("Connection initiated to " + config_.broker_host + ":" + std::to_string(config_.broker_port));
        return true;
    } else {
        LogError("Failed to initiate connection: " + std::string(mosquitto_strerror(result)));
        UpdateConnectionStatus(ConnectionStatus::ERROR);
        return false;
    }
}

void MQTTOnlyBridge::LogInfo(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

void MQTTOnlyBridge::LogError(const std::string& message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

void MQTTOnlyBridge::LogDebug(const std::string& message) {
    std::cout << "[DEBUG] " << message << std::endl;
}

void MQTTOnlyBridge::LogWarning(const std::string& message) {
    std::cout << "[WARNING] " << message << std::endl;
}


//=============================================================================
// MQTTOnlyUtils Implementation
//=============================================================================

namespace MQTTOnlyUtils {

std::string BuildSensorTopic(const std::string& prefix, const std::string& sensor_id, const std::string& data_type) {
    return prefix + sensor_id + "/" + data_type;
}

std::string BuildAlertTopic(const std::string& prefix, const std::string& alert_type) {
    return prefix + alert_type;
}

std::string BuildCommandTopic(const std::string& prefix, const std::string& target) {
    return prefix + target;
}

std::string BuildStatusTopic(const std::string& prefix, const std::string& component) {
    return prefix + component;
}

std::string CreateSensorDataPayload(const std::string& sensor_id, double temperature, 
                                   double humidity, const std::string& location) {
    Json::Value json;
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

std::string CreateAlertPayload(const std::string& sensor_id, const std::string& alert_type,
                              const std::string& message, double value) {
    Json::Value json;
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

bool ParseSensorDataPayload(const std::string& payload, std::string& sensor_id, 
                           double& temperature, double& humidity, std::string& location) {
    try {
        Json::Value json;
        Json::CharReaderBuilder reader_builder;
        std::string errors;
        std::stringstream ss(payload);
        
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
    } catch (const std::exception& e) {
        return false;
    }
}

double CalculateThroughput(uint64_t message_count, std::chrono::milliseconds duration) {
    if (duration.count() == 0) return 0.0;
    return (static_cast<double>(message_count) * 1000.0) / duration.count();
}

std::string FormatMetrics(const MQTTOnlyBridge::Metrics& metrics) {
    std::stringstream ss;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.start_time);
    
    ss << "=== MQTT-Only Bridge Metrics ===\n";
    ss << "Uptime: " << duration.count() << " seconds\n";
    ss << "Messages Received: " << metrics.messages_received.load() << "\n";
    ss << "Messages Published: " << metrics.messages_published.load() << "\n";
    ss << "Messages Processed: " << metrics.messages_processed.load() << "\n";
    ss << "Messages Failed: " << metrics.messages_failed.load() << "\n";
    ss << "Current Queue Size: " << metrics.current_queue_size.load() << "\n";
    ss << "Max Queue Size: " << metrics.max_queue_size.load() << "\n";
    ss << "Avg Processing Time: " << std::fixed << std::setprecision(2) 
       << metrics.avg_processing_time_ms.load() << " ms\n";
    ss << "Max Processing Time: " << std::fixed << std::setprecision(2) 
       << metrics.max_processing_time_ms.load() << " ms\n";
    ss << "Connection Status: " << (metrics.is_connected.load() ? "Connected" : "Disconnected") << "\n";
    ss << "Throughput: " << std::fixed << std::setprecision(1) 
       << CalculateThroughput(metrics.messages_received.load(), 
                            std::chrono::duration_cast<std::chrono::milliseconds>(duration))
       << " msg/sec\n";
    
    return ss.str();
}

MQTTOnlyBridge::Config CreateDefaultConfig() {
    MQTTOnlyBridge::Config config;
    config.broker_host = "localhost";
    config.broker_port = 1883;
    config.client_id = "mqtt_only_bridge_default";
    config.keepalive = 60;
    config.worker_thread_count = 4;
    return config;
}

MQTTOnlyBridge::Config CreateHighThroughputConfig() {
    MQTTOnlyBridge::Config config;
    config.broker_host = "localhost";
    config.broker_port = 1883;
    config.client_id = "mqtt_only_bridge_high_throughput";
    config.keepalive = 30;
    config.sensor_data_qos = 0;  // Fire-and-forget for speed
    config.max_inflight_messages = 2000;
    config.worker_thread_count = 8;
    config.message_buffer_size = 20000;
    return config;
}

MQTTOnlyBridge::Config CreateLowLatencyConfig() {
    MQTTOnlyBridge::Config config;
    config.broker_host = "localhost";
    config.broker_port = 1883;
    config.client_id = "mqtt_only_bridge_low_latency";
    config.keepalive = 15;
    config.sensor_data_qos = 1;  // At-least-once for reliability
    config.max_inflight_messages = 100;
    config.worker_thread_count = 2;
    config.message_buffer_size = 1000;
    return config;
}

bool ValidateBrokerConnection(const std::string& host, int port, std::chrono::seconds timeout) {
    struct mosquitto* test_mosq = mosquitto_new("test_connection", true, nullptr);
    if (!test_mosq) return false;
    
    bool connected = false;
    
    mosquitto_connect_callback_set(test_mosq, [](struct mosquitto*, void* userdata, int result) {
        bool* conn_ptr = static_cast<bool*>(userdata);
        *conn_ptr = (result == 0);
    });
    
    int result = mosquitto_connect(test_mosq, host.c_str(), port, 60);
    if (result == MOSQ_ERR_SUCCESS) {
        auto start_time = std::chrono::steady_clock::now();
        while (!connected && 
               std::chrono::steady_clock::now() - start_time < timeout) {
            mosquitto_loop(test_mosq, 100, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    mosquitto_disconnect(test_mosq);
    mosquitto_destroy(test_mosq);
    return connected;
}

void RunPerformanceTest(MQTTOnlyBridge& bridge, int duration_seconds, int messages_per_second) {
    std::cout << "Starting performance test..." << std::endl;
    std::cout << "Duration: " << duration_seconds << " seconds" << std::endl;
    std::cout << "Target rate: " << messages_per_second << " msg/sec" << std::endl;
    
    auto start_metrics = bridge.GetMetrics();
    auto start_time = std::chrono::steady_clock::now();
    
    int message_interval_us = 1000000 / messages_per_second;
    int total_messages = duration_seconds * messages_per_second;
    
    for (int i = 0; i < total_messages && bridge.IsRunning(); ++i) {
        Json::Value sensor_data;
        sensor_data["temperature"] = 20.0 + (rand() % 100) / 10.0;
        sensor_data["humidity"] = 40.0 + (rand() % 400) / 10.0;
        sensor_data["location"] = "test_room";
        
        std::string sensor_id = "test_sensor_" + std::to_string(i % 10);
        bridge.PublishSensorData(sensor_id, sensor_data);
        
        std::this_thread::sleep_for(std::chrono::microseconds(message_interval_us));
    }
    
    // Wait for processing to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auto end_time = std::chrono::steady_clock::now();
    auto end_metrics = bridge.GetMetrics();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    uint64_t messages_sent = end_metrics.messages_published - start_metrics.messages_published;
    uint64_t messages_processed = end_metrics.messages_processed - start_metrics.messages_processed;
    
    std::cout << "\n=== Performance Test Results ===" << std::endl;
    std::cout << "Actual duration: " << duration.count() << " ms" << std::endl;
    std::cout << "Messages sent: " << messages_sent << std::endl;
    std::cout << "Messages processed: " << messages_processed << std::endl;
    std::cout << "Actual throughput: " << CalculateThroughput(messages_sent, duration) << " msg/sec" << std::endl;
    std::cout << "Processing throughput: " << CalculateThroughput(messages_processed, duration) << " msg/sec" << std::endl;
    std::cout << "Success rate: " << (100.0 * messages_processed / messages_sent) << "%" << std::endl;
}

} // namespace MQTTOnlyUtils

// Additional MQTTOnlyBridge methods

bool MQTTOnlyBridge::Subscribe(const std::string& topic, int qos) {
    if (!mosq_ || !IsConnected()) return false;
    
    if (qos == -1) qos = config_.default_qos;
    
    int mid;
    int result = mosquitto_subscribe(mosq_, &mid, topic.c_str(), qos);
    
    if (result == MOSQ_ERR_SUCCESS) {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        active_subscriptions_.push_back(topic);
        LogInfo("Subscribed to: " + topic);
        return true;
    } else {
        LogError("Failed to subscribe to " + topic + ": " + std::string(mosquitto_strerror(result)));
        return false;
    }
}

bool MQTTOnlyBridge::SubscribeToSensorData(const std::string& sensor_pattern) {
    std::string topic = config_.sensor_topic_prefix + sensor_pattern + "/data";
    return Subscribe(topic, config_.sensor_data_qos);
}

MQTTOnlyBridge::Metrics MQTTOnlyBridge::GetMetrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

bool MQTTOnlyBridge::IsConnected() const {
    return connection_status_.load() == ConnectionStatus::CONNECTED;
}

double MQTTOnlyBridge::GetCurrentThroughput() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics_.start_time);
    return MQTTOnlyUtils::CalculateThroughput(metrics_.messages_received.load(), duration);
}

bool MQTTOnlyBridge::EnableThermalMonitoring(const ThermalConfig& thermal_config) {
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
            
            // Publish alert via MQTT
            std::string alert_topic = MQTTOnlyUtils::BuildAlertTopic(config_.alert_topic_prefix, "thermal");
            std::string alert_payload = MQTTOnlyUtils::CreateAlertPayload(
                alert.sensor_id, "thermal_alert", "", alert.temperature);
            
            PublishMessage(alert_topic, alert_payload, config_.alerts_qos);
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

void MQTTOnlyBridge::DisableThermalMonitoring() {
    if (!thermal_enabled_.load()) return;
    
    thermal_enabled_ = false;
    if (thermal_tracker_) {
        thermal_tracker_->stop();
        thermal_tracker_.reset();
    }
    LogInfo("Thermal monitoring disabled");
}

void MQTTOnlyBridge::UpdateConnectionStatus(ConnectionStatus status, const std::string& message) {
    connection_status_ = status;
    if (connection_callback_) {
        connection_callback_(status, message);
    }
}

void MQTTOnlyBridge::UpdateProcessingMetrics(double processing_time_ms) {
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

bool MQTTOnlyBridge::IsTopicAllowed(const std::string& topic) {
    if (!config_.enable_topic_filtering) return true;
    
    std::lock_guard<std::mutex> lock(topic_filter_mutex_);
    
    // Check blocked patterns first
    for (const auto& pattern : blocked_topic_patterns_) {
        if (MatchesPattern(topic, pattern)) {
            return false;
        }
    }
    
    // If no allowed patterns specified, allow all (except blocked)
    if (allowed_topic_patterns_.empty()) return true;
    
    // Check allowed patterns
    for (const auto& pattern : allowed_topic_patterns_) {
        if (MatchesPattern(topic, pattern)) {
            return true;
        }
    }
    
    return false;
}

bool MQTTOnlyBridge::MatchesPattern(const std::string& topic, const std::string& pattern) {
    // Simple wildcard matching for MQTT topics
    // + matches single level, # matches multiple levels
    
    if (pattern == "#") return true;
    if (pattern == topic) return true;
    
    // For now, implement basic matching
    // In production, use proper MQTT topic matching algorithm
    return topic.find(pattern.substr(0, pattern.find('+'))) == 0;
}

void MQTTOnlyBridge::MetricsLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        if (running_.load()) {
            LogDebug("Metrics: " + std::to_string(metrics_.messages_received.load()) + 
                    " received, " + std::to_string(metrics_.messages_processed.load()) + " processed");
        }
    }
}

void MQTTOnlyBridge::HeartbeatLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        if (running_.load() && IsConnected()) {
            PublishHeartbeat(config_.client_id);
        }
    }
}

bool MQTTOnlyBridge::PublishHeartbeat(const std::string& client_id) {
    std::string topic = MQTTOnlyUtils::BuildHeartbeatTopic("heartbeat/", client_id);
    std::string payload = MQTTOnlyUtils::CreateHeartbeatPayload(client_id);
    return PublishMessage(topic, payload, 0, false);
}

void MQTTOnlyBridge::CleanupMosquitto() {
    if (mosq_) {
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
    }
}

// Static callback implementations
void MQTTOnlyBridge::OnPublish(struct mosquitto* mosq, void* userdata, int mid) {
    // Message published successfully
}

void MQTTOnlyBridge::OnSubscribe(struct mosquitto* mosq, void* userdata, int mid, int qos_count, const int* granted_qos) {
    MQTTOnlyBridge* bridge = static_cast<MQTTOnlyBridge*>(userdata);
    bridge->LogDebug("Subscription confirmed");
}

void MQTTOnlyBridge::OnLog(struct mosquitto* mosq, void* userdata, int level, const char* str) {
    MQTTOnlyBridge* bridge = static_cast<MQTTOnlyBridge*>(userdata);
    
    switch (level) {
        case MOSQ_LOG_ERR:
            bridge->LogError("Mosquitto: " + std::string(str));
            break;
        case MOSQ_LOG_WARNING:
            bridge->LogWarning("Mosquitto: " + std::string(str));
            break;
        case MOSQ_LOG_DEBUG:
        case MOSQ_LOG_INFO:
            bridge->LogDebug("Mosquitto: " + std::string(str));
            break;
    }
}

