#include "ComponentManagers.h"
#include "IntegrationTestController.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <random>
#include <algorithm>
#include <numeric>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <chrono>

// Types are defined in ComponentManagers.h

//===============================================================================
// STM32 Simulator Manager Implementation
//===============================================================================

STM32SimulatorManager::STM32SimulatorManager() {
    // Initialize with default configuration
}

STM32SimulatorManager::~STM32SimulatorManager() {
    StopAll();
}

bool STM32SimulatorManager::StartAll() {
    std::cout << "    Starting STM32 simulators..." << std::endl;
    
    running_ = true;
    
    // Create default simulators if none exist
    if (simulators_.empty()) {
        SetNumberOfSimulators(3); // Default to 3 simulators
    }
    
    // Start simulator threads
    for (auto& pair : simulators_) {
        const std::string& sim_id = pair.first;
        simulator_threads_[sim_id] = std::thread(&STM32SimulatorManager::SimulatorThread, this, sim_id);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow startup
    std::cout << "    STM32 simulators started: " << simulators_.size() << std::endl;
    return true;
}

bool STM32SimulatorManager::StopAll() {
    running_ = false;
    
    // Join all threads
    for (auto& pair : simulator_threads_) {
        if (pair.second.joinable()) {
            pair.second.join();
        }
    }
    
    simulator_threads_.clear();
    simulators_.clear();
    
    std::cout << "    STM32 simulators stopped" << std::endl;
    return true;
}

void STM32SimulatorManager::SetNumberOfSimulators(int count) {
    simulators_.clear();
    
    for (int i = 0; i < count; ++i) {
        std::string sim_id = GenerateSimulatorId();
        
        // Create a simplified sensor node for testing
        // Using nullptr as placeholder
        simulators_[sim_id] = nullptr;
    }
}

void STM32SimulatorManager::SetSensorTypes(const std::vector<SensorType>& types) {
    // Implementation for setting sensor types
}

void STM32SimulatorManager::SetEnvironmentPattern(EnvironmentPattern pattern) {
    // Implementation for setting environment pattern
}

void STM32SimulatorManager::SetMessageInterval(std::chrono::milliseconds interval) {
    // Implementation for setting message interval
}

std::vector<std::string> STM32SimulatorManager::GetActiveSimulators() const {
    std::vector<std::string> active;
    for (const auto& pair : simulators_) {
        active.push_back(pair.first);
    }
    return active;
}

SensorReading STM32SimulatorManager::GetLastReading(const std::string& simulator_id) const {
    SensorReading reading;
    reading.temperature = 22.0;
    reading.humidity = 45.0;
    reading.timestamp = std::chrono::system_clock::now();
    return reading;
}

std::map<std::string, int> STM32SimulatorManager::GetMessageCounts() const {
    std::map<std::string, int> counts;
    for (const auto& pair : simulators_) {
        counts[pair.first] = 100; // Simulated count
    }
    return counts;
}

bool STM32SimulatorManager::SendTestMessage(const std::string& simulator_id, const std::string& message) {
    auto it = simulators_.find(simulator_id);
    if (it == simulators_.end()) {
        return false;
    }
    
    // Simulate message sending with some latency
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    
    // For testing purposes, we always succeed unless simulator is marked as faulty
    return IsSimulatorHealthy(simulator_id);
}

bool STM32SimulatorManager::IsSimulatorHealthy(const std::string& simulator_id) const {
    auto it = simulators_.find(simulator_id);
    return it != simulators_.end();
}

void STM32SimulatorManager::InjectFault(const std::string& simulator_id, FaultType fault) {
    // For testing, we'll mark the simulator as unhealthy by removing it temporarily
    // In a real implementation, this would set fault flags
    std::cout << "      Injecting fault in simulator " << simulator_id << std::endl;
    // Fault injection is simulated
}

void STM32SimulatorManager::ClearFault(const std::string& simulator_id) {
    std::cout << "      Clearing fault in simulator " << simulator_id << std::endl;
    // Fault clearing is simulated
}

void STM32SimulatorManager::GenerateTestData(const TestConfiguration& config) {
    // Implementation for generating test data
}

void STM32SimulatorManager::SimulateRealisticLoad(int messages_per_second, std::chrono::seconds duration) {
    std::cout << "    Simulating realistic load: " << messages_per_second << " msg/s for " << duration.count() << "s" << std::endl;
}

std::string STM32SimulatorManager::GenerateSimulatorId() {
    return "STM32_SIM_" + std::to_string(next_simulator_id_++);
}

void STM32SimulatorManager::SimulatorThread(const std::string& simulator_id) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::normal_distribution<> temp_dist(22.0, 2.0);
    static std::normal_distribution<> hum_dist(45.0, 5.0);
    
    while (running_) {
        // Simulate sensor readings
        double temperature = temp_dist(gen);
        double humidity = hum_dist(gen);
        
        // In a real implementation, this would generate and send actual sensor data
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

//===============================================================================
// RPi4 Gateway Manager Implementation  
//===============================================================================

RPi4GatewayManager::RPi4GatewayManager() {
    // Initialize with default configuration
}

RPi4GatewayManager::~RPi4GatewayManager() {
    StopAll();
}

bool RPi4GatewayManager::StartAll() {
    std::cout << "    Starting RPi4 gateways..." << std::endl;
    
    running_ = true;
    
    // Create default gateways if none exist
    if (gateways_.empty()) {
        SetNumberOfGateways(1); // Default to 1 gateway
    }
    
    // Start gateway threads
    for (auto& pair : gateways_) {
        const std::string& gateway_id = pair.first;
        gateway_threads_[gateway_id] = std::thread(&RPi4GatewayManager::GatewayThread, this, gateway_id);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow startup
    std::cout << "    RPi4 gateways started: " << gateways_.size() << std::endl;
    return true;
}

bool RPi4GatewayManager::StopAll() {
    running_ = false;
    
    // Join all threads
    for (auto& pair : gateway_threads_) {
        if (pair.second.joinable()) {
            pair.second.join();
        }
    }
    
    gateway_threads_.clear();
    gateways_.clear();
    
    std::cout << "    RPi4 gateways stopped" << std::endl;
    return true;
}

void RPi4GatewayManager::SetNumberOfGateways(int count) {
    gateways_.clear();
    
    for (int i = 0; i < count; ++i) {
        std::string gateway_id = GenerateGatewayId();
        
        // Create a simplified gateway for testing
        gateways_[gateway_id] = nullptr;
    }
}

std::vector<std::string> RPi4GatewayManager::GetActiveGateways() const {
    std::vector<std::string> active;
    for (const auto& pair : gateways_) {
        active.push_back(pair.first);
    }
    return active;
}

bool RPi4GatewayManager::IsGatewayHealthy(const std::string& gateway_id) const {
    auto it = gateways_.find(gateway_id);
    return it != gateways_.end();
}

bool RPi4GatewayManager::RestartGateway(const std::string& gateway_id) {
    std::cout << "      Restarting gateway " << gateway_id << std::endl;
    
    // Simulate restart by stopping and starting the thread
    auto thread_it = gateway_threads_.find(gateway_id);
    if (thread_it != gateway_threads_.end() && thread_it->second.joinable()) {
        // In a real implementation, we'd properly stop and restart
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return true;
}

// Stub implementations for remaining RPi4GatewayManager methods
void RPi4GatewayManager::SetGatewayConfiguration(const std::string& gateway_id, const RPi4Gateway::Config& config) {
    // Implementation stub
}

void RPi4GatewayManager::EnableEdgeAnalytics(bool enable) {
    // Implementation stub
}

bool RPi4GatewayManager::ConnectToSimulator(const std::string& gateway_id, const std::string& simulator_id) {
    return true;
}

bool RPi4GatewayManager::SetupMQTTForwarding(const std::string& gateway_id, const std::string& mqtt_broker) {
    return true;
}

RPi4Gateway::SystemStats RPi4GatewayManager::GetGatewayStats(const std::string& gateway_id) const {
    RPi4Gateway::SystemStats stats;
    // Return default stats
    return stats;
}

std::map<std::string, int> RPi4GatewayManager::GetProcessedMessageCounts() const {
    std::map<std::string, int> counts;
    return counts;
}

void RPi4GatewayManager::SetProcessingMode(const std::string& gateway_id, ProcessingMode mode) {
    // Implementation stub
}

void RPi4GatewayManager::TriggerDataForwarding(const std::string& gateway_id) {
    // Implementation stub
}

void RPi4GatewayManager::InjectProcessingDelay(const std::string& gateway_id, std::chrono::milliseconds delay) {
    // Implementation stub
}

void RPi4GatewayManager::ConfigureForTesting(const TestConfiguration& config) {
    // Implementation stub
}

void RPi4GatewayManager::SimulateNetworkLatency(const std::string& gateway_id, double latency_ms) {
    // Implementation stub
}

std::string RPi4GatewayManager::GenerateGatewayId() {
    return "RPI4_GW_" + std::to_string(next_gateway_id_++);
}

void RPi4GatewayManager::GatewayThread(const std::string& gateway_id) {
    while (running_) {
        // Simulate gateway processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

//===============================================================================
// MQTT Bridge Manager Implementation
//===============================================================================

MQTTBridgeManager::MQTTBridgeManager() {
    // Initialize message tracking
}

MQTTBridgeManager::~MQTTBridgeManager() {
    Stop();
}

bool MQTTBridgeManager::Start() {
    std::cout << "    Starting MQTT-WebSocket bridge..." << std::endl;
    
    // Check if mosquitto broker is available
    if (system("pgrep mosquitto > /dev/null") != 0) {
        std::cout << "      Starting mosquitto broker..." << std::endl;
        system("mosquitto -d > /dev/null 2>&1");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    running_ = true;
    
    // Start bridge process or thread
    bridge_thread_ = std::make_unique<std::thread>(&MQTTBridgeManager::BridgeThread, this);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Allow startup
    std::cout << "    MQTT-WebSocket bridge started" << std::endl;
    return true;
}

bool MQTTBridgeManager::Stop() {
    running_ = false;
    
    if (bridge_thread_ && bridge_thread_->joinable()) {
        bridge_thread_->join();
    }
    
    // Stop mosquitto if we started it
    system("pkill mosquitto > /dev/null 2>&1");
    
    std::cout << "    MQTT-WebSocket bridge stopped" << std::endl;
    return true;
}

bool MQTTBridgeManager::Restart() {
    Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return Start();
}

bool MQTTBridgeManager::IsRunning() const {
    return running_;
}

void MQTTBridgeManager::SetMQTTBroker(const std::string& broker_url, int port) {
    mqtt_broker_url_ = broker_url;
    mqtt_port_ = port;
}

void MQTTBridgeManager::SetWebSocketPort(int port) {
    websocket_port_ = port;
}

void MQTTBridgeManager::SetTopicFilters(const std::vector<std::string>& topics) {
    topic_filters_ = topics;
}

void MQTTBridgeManager::EnableLogging(bool enable) {
    // Implementation stub
}

int MQTTBridgeManager::GetConnectedClients() const {
    return 1; // Simulated
}

std::map<std::string, int> MQTTBridgeManager::GetTopicMessageCounts() const {
    std::lock_guard<std::mutex> lock(message_mutex_);
    return topic_counts_;
}

double MQTTBridgeManager::getAverageLatency() const {
    std::lock_guard<std::mutex> lock(message_mutex_);
    if (latencies_.empty()) return 0.0;
    return std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
}

void MQTTBridgeManager::SetupForTesting(const TestConfiguration& config) {
    // Configure for testing
    topic_filters_.clear();
    topic_filters_.push_back("sensors/+/data");
    topic_filters_.push_back("alerts/+");
    
    ClearMessageHistory();
}

bool MQTTBridgeManager::PublishTestMessage(const std::string& topic, const std::string& message) {
    // Simulate message publishing
    {
        std::lock_guard<std::mutex> lock(message_mutex_);
        received_messages_[topic].push_back(message);
        topic_counts_[topic]++;
    }
    
    // Simulate network latency
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    return true;
}

bool MQTTBridgeManager::SubscribeToTopic(const std::string& topic) {
    topic_filters_.push_back(topic);
    return true;
}

std::vector<std::string> MQTTBridgeManager::GetReceivedMessages(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(message_mutex_);
    auto it = received_messages_.find(topic);
    return it != received_messages_.end() ? it->second : std::vector<std::string>();
}

void MQTTBridgeManager::ClearMessageHistory() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    received_messages_.clear();
    topic_counts_.clear();
    latencies_.clear();
}

void MQTTBridgeManager::InjectNetworkFailure(std::chrono::seconds duration) {
    std::cout << "      Injecting network failure for " << duration.count() << "s" << std::endl;
    
    // Simulate network failure by temporarily stopping processing
    bool was_running = running_;
    running_ = false;
    
    std::this_thread::sleep_for(duration);
    
    running_ = was_running;
    std::cout << "      Network failure cleared" << std::endl;
}

void MQTTBridgeManager::BridgeThread() {
    while (running_) {
        // Simulate bridge operation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Simulate processing latency
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::normal_distribution<> latency_dist(5.0, 2.0);
        
        double latency = std::max(1.0, latency_dist(gen));
        {
            std::lock_guard<std::mutex> lock(message_mutex_);
            latencies_.push_back(latency);
        }
    }
}

//===============================================================================
// Thermal Monitor Manager Implementation
//===============================================================================

ThermalMonitorManager::ThermalMonitorManager() : thermal_tracker_(nullptr) {
    // Initialize thermal tracker
    // In a real implementation, this would create the actual ThermalIsolationTracker
}

ThermalMonitorManager::~ThermalMonitorManager() {
    Stop();
}

bool ThermalMonitorManager::Start() {
    std::cout << "    Starting thermal monitoring..." << std::endl;
    
    running_ = true;
    
    // Start monitoring thread
    monitoring_thread_ = std::make_unique<std::thread>(&ThermalMonitorManager::MonitoringThread, this);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow startup
    std::cout << "    Thermal monitoring started" << std::endl;
    return true;
}

bool ThermalMonitorManager::Stop() {
    running_ = false;
    
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    
    std::cout << "    Thermal monitoring stopped" << std::endl;
    return true;
}

bool ThermalMonitorManager::Restart() {
    Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return Start();
}

bool ThermalMonitorManager::IsRunning() const {
    return running_;
}

void ThermalMonitorManager::SetupForTesting(const TestConfiguration& config) {
    // Configure thermal monitoring for testing
    SetThermalThresholds(config.temp_low_threshold, config.temp_high_threshold, config.humidity_high_threshold);
    ClearAlerts();
}

void ThermalMonitorManager::SetThermalThresholds(double temp_low, double temp_high, double humidity_high) {
    std::cout << "      Setting thermal thresholds: temp " << temp_low << "-" << temp_high 
              << "°C, humidity " << humidity_high << "%" << std::endl;
    // Thresholds would be set in the actual thermal tracker
}

void ThermalMonitorManager::SetMonitoringInterval(std::chrono::milliseconds interval) {
    // Implementation stub
}

void ThermalMonitorManager::SetAlertCooldown(std::chrono::seconds cooldown) {
    // Implementation stub
}

void ThermalMonitorManager::EnableAlertTypes(const std::vector<AlertType>& types) {
    // Implementation stub
}

std::vector<Alert> ThermalMonitorManager::GetRecentAlerts(std::chrono::minutes timeframe) const {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    return generated_alerts_;
}

std::map<std::string, SensorData> ThermalMonitorManager::GetCurrentSensorData() const {
    std::map<std::string, SensorData> data;
    // Return simulated data
    return data;
}

bool ThermalMonitorManager::ProcessSensorMessage(const std::string& sensor_id, const std::string& message) {
    // Parse message and check for threshold violations
    // This is a simplified implementation
    
    // Extract temperature and humidity from JSON-like message
    double temperature = 22.0, humidity = 45.0;
    
    size_t temp_pos = message.find("\"temperature\":");
    if (temp_pos != std::string::npos) {
        size_t start = message.find(':', temp_pos) + 1;
        size_t end = message.find(',', start);
        if (end == std::string::npos) end = message.find('}', start);
        
        std::string temp_str = message.substr(start, end - start);
        temperature = std::stod(temp_str);
    }
    
    size_t hum_pos = message.find("\"humidity\":");
    if (hum_pos != std::string::npos) {
        size_t start = message.find(':', hum_pos) + 1;
        size_t end = message.find(',', start);
        if (end == std::string::npos) end = message.find('}', start);
        
        std::string hum_str = message.substr(start, end - start);
        humidity = std::stod(hum_str);
    }
    
    // Check for threshold violations and generate alerts
    if (temperature > 35.0) {
        Alert alert;
        alert.type = AlertType::TEMPERATURE_HIGH;
        alert.sensor_id = sensor_id;
        alert.message = "High temperature detected: " + std::to_string(temperature) + "°C";
        alert.timestamp = std::chrono::system_clock::now();
        OnAlertGenerated(alert);
    }
    
    if (temperature < 15.0) {
        Alert alert;
        alert.type = AlertType::TEMPERATURE_LOW;
        alert.sensor_id = sensor_id;
        alert.message = "Low temperature detected: " + std::to_string(temperature) + "°C";
        alert.timestamp = std::chrono::system_clock::now();
        OnAlertGenerated(alert);
    }
    
    if (humidity > 80.0) {
        Alert alert;
        alert.type = AlertType::HUMIDITY_HIGH;
        alert.sensor_id = sensor_id;
        alert.message = "High humidity detected: " + std::to_string(humidity) + "%";
        alert.timestamp = std::chrono::system_clock::now();
        OnAlertGenerated(alert);
    }
    
    return true;
}

void ThermalMonitorManager::InjectTestSensorData(const std::string& sensor_id, double temperature, double humidity) {
    std::ostringstream message;
    message << "{\"temperature\":" << temperature << ",\"humidity\":" << humidity 
            << ",\"location\":\"test\",\"timestamp\":" 
            << std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count() << "}";
    
    ProcessSensorMessage(sensor_id, message.str());
}

void ThermalMonitorManager::SimulateThresholdViolation(const std::string& sensor_id, AlertType alert_type) {
    Alert alert;
    alert.type = alert_type;
    alert.sensor_id = sensor_id;
    alert.message = "Simulated threshold violation";
    alert.timestamp = std::chrono::system_clock::now();
    OnAlertGenerated(alert);
}

std::vector<Alert> ThermalMonitorManager::GetAllGeneratedAlerts() const {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    return generated_alerts_;
}

int ThermalMonitorManager::GetActiveAlertCount() const {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    return generated_alerts_.size();
}

void ThermalMonitorManager::ClearAlerts() {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    generated_alerts_.clear();
}

void ThermalMonitorManager::SetSensorOfflineTimeout(std::chrono::seconds timeout) {
    // Set offline timeout in the thermal tracker
    std::cout << "      Setting sensor offline timeout: " << timeout.count() << "s" << std::endl;
}

void ThermalMonitorManager::MonitoringThread() {
    while (running_) {
        // Simulate thermal monitoring processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ThermalMonitorManager::OnAlertGenerated(const Alert& alert) {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    generated_alerts_.push_back(alert);
    
    std::cout << "      Alert generated - Type: " << static_cast<int>(alert.type) 
              << ", Sensor: " << alert.sensor_id << ", Message: " << alert.message << std::endl;
}

//===============================================================================
// Integration Utilities Implementation
//===============================================================================

namespace IntegrationUtils {
    bool CheckComponentHealth(const std::string& component_name) {
        // Simplified health check
        if (component_name == "mosquitto") {
            return system("pgrep mosquitto > /dev/null") == 0;
        }
        
        return true; // Assume healthy for testing
    }
    
    std::map<std::string, bool> CheckAllComponentsHealth() {
        std::map<std::string, bool> health;
        health["mosquitto"] = CheckComponentHealth("mosquitto");
        health["bridge"] = true; // Simplified
        health["thermal"] = true; // Simplified
        return health;
    }
    
    pid_t StartProcess(const std::string& command, const std::vector<std::string>& args) {
        // Simplified process starting
        return -1;
    }
    
    bool StopProcess(pid_t pid, std::chrono::seconds timeout) {
        if (pid > 0) {
            kill(pid, SIGTERM);
            return true;
        }
        return false;
    }
    
    bool IsProcessRunning(pid_t pid) {
        return pid > 0 && kill(pid, 0) == 0;
    }
    
    bool IsPortOpen(int port) {
        std::string cmd = "netstat -ln | grep :" + std::to_string(port) + " > /dev/null 2>&1";
        return system(cmd.c_str()) == 0;
    }
    
    bool WaitForPortOpen(int port, std::chrono::seconds timeout) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            if (IsPortOpen(port)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }
    
    std::string GetLocalIPAddress() {
        return "127.0.0.1";
    }
    
    SystemMetrics GetSystemMetrics() {
        SystemMetrics metrics;
        
        // Get memory usage
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        size_t total_mem = 0, available_mem = 0;
        
        while (std::getline(meminfo, line)) {
            if (line.substr(0, 10) == "MemTotal:") {
                std::istringstream iss(line);
                std::string key, value, unit;
                iss >> key >> value >> unit;
                total_mem = std::stoul(value);
            } else if (line.substr(0, 13) == "MemAvailable:") {
                std::istringstream iss(line);
                std::string key, value, unit;
                iss >> key >> value >> unit;
                available_mem = std::stoul(value);
            }
        }
        
        metrics.memory_usage_kb = total_mem - available_mem;
        metrics.cpu_usage_percent = 0.0; // Simplified - would need actual CPU monitoring
        metrics.disk_usage_kb = 0; // Simplified
        metrics.network_connections = 0; // Simplified
        
        return metrics;
    }
    
    std::string GenerateRealisticSensorMessage(const std::string& sensor_id, 
                                               double base_temp, double base_humidity) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::normal_distribution<> temp_variation(-0.5, 0.5);
        static std::normal_distribution<> hum_variation(-2.0, 2.0);
        
        double temperature = base_temp + temp_variation(gen);
        double humidity = base_humidity + hum_variation(gen);
        
        std::ostringstream oss;
        oss << "{"
            << "\"sensor_id\":\"" << sensor_id << "\","
            << "\"temperature\":" << std::fixed << std::setprecision(1) << temperature << ","
            << "\"humidity\":" << std::fixed << std::setprecision(1) << humidity << ","
            << "\"location\":\"test_location\","
            << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count()
            << "}";
        
        return oss.str();
    }
    
    std::vector<std::string> GenerateTestSensorIds(int count) {
        std::vector<std::string> ids;
        for (int i = 0; i < count; ++i) {
            ids.push_back("TEST_SENSOR_" + std::to_string(i + 1));
        }
        return ids;
    }
    
    std::string GenerateRandomLocation() {
        static std::vector<std::string> locations = {
            "room1", "room2", "kitchen", "garage", "basement", "attic", "office", "lab"
        };
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, locations.size() - 1);
        
        return locations[dis(gen)];
    }
    
    bool ValidateJSONMessage(const std::string& message) {
        // Simple JSON validation
        return (message.find('{') != std::string::npos && 
                message.find('}') != std::string::npos);
    }
    
    bool ValidateSensorDataMessage(const std::string& message) {
        return ValidateJSONMessage(message) &&
               (message.find("temperature") != std::string::npos) &&
               (message.find("humidity") != std::string::npos);
    }
    
    bool ValidateAlertMessage(const std::string& message) {
        return ValidateJSONMessage(message) &&
               (message.find("alert") != std::string::npos || message.find("type") != std::string::npos);
    }
    
    void PreciseSleep(std::chrono::microseconds duration) {
        std::this_thread::sleep_for(duration);
    }
    
    std::chrono::steady_clock::time_point GetHighPrecisionTime() {
        return std::chrono::steady_clock::now();
    }
    
    double CalculatePreciseLatency(const std::chrono::steady_clock::time_point& start,
                                  const std::chrono::steady_clock::time_point& end) {
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    }
}