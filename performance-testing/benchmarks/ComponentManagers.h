#ifndef COMPONENT_MANAGERS_H
#define COMPONENT_MANAGERS_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
// Forward declarations - simplified for compilation
struct TestConfiguration;
struct TestMetrics;

// Define basic types for compilation
enum class SensorType { DHT22, BME280, SHT30, DS18B20 };
enum class EnvironmentPattern { INDOOR, OUTDOOR, INDUSTRIAL };
enum class FaultType { COMMUNICATION_ERROR, POWER_FAILURE, SENSOR_FAILURE };
enum class ProcessingMode { NORMAL, HIGH_PERFORMANCE, LOW_POWER };
enum class AlertType { 
    TEMPERATURE_HIGH = 0, 
    TEMPERATURE_LOW = 1, 
    HUMIDITY_HIGH = 2, 
    TEMPERATURE_RISING_FAST = 3, 
    TEMPERATURE_FALLING_FAST = 4, 
    SENSOR_OFFLINE = 5 
};

struct SensorReading {
    double temperature;
    double humidity;
    std::chrono::system_clock::time_point timestamp;
};

struct Alert {
    AlertType type;
    std::string sensor_id;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

struct SensorData {
    std::string sensor_id;
    SensorReading last_reading;
    bool is_online;
};

// Simplified class declarations for compilation
class STM32SensorNode;
class RPi4Gateway {
public:
    struct Config {};
    struct SystemStats {};
};
class ThermalIsolationTracker;

// STM32 Simulator Manager
class STM32SimulatorManager {
public:
    STM32SimulatorManager();
    ~STM32SimulatorManager();
    
    bool StartAll();
    bool StopAll();
    bool RestartSimulator(const std::string& simulator_id);
    
    // Configuration
    void SetNumberOfSimulators(int count);
    void SetSensorTypes(const std::vector<SensorType>& types);
    void SetEnvironmentPattern(EnvironmentPattern pattern);
    void SetMessageInterval(std::chrono::milliseconds interval);
    
    // Control
    bool SendTestMessage(const std::string& simulator_id, const std::string& message);
    void InjectFault(const std::string& simulator_id, FaultType fault);
    void ClearFault(const std::string& simulator_id);
    
    // Monitoring
    std::vector<std::string> GetActiveSimulators() const;
    SensorReading GetLastReading(const std::string& simulator_id) const;
    std::map<std::string, int> GetMessageCounts() const;
    bool IsSimulatorHealthy(const std::string& simulator_id) const;
    
    // Test utilities
    void GenerateTestData(const TestConfiguration& config);
    void SimulateRealisticLoad(int messages_per_second, std::chrono::seconds duration);
    
private:
    std::map<std::string, void*> simulators_;  // Using void* to avoid incomplete type issues
    std::map<std::string, std::thread> simulator_threads_;
    std::atomic<bool> running_{false};
    int next_simulator_id_ = 1;
    
    void SimulatorThread(const std::string& simulator_id);
    std::string GenerateSimulatorId();
};

// RPi4 Gateway Manager
class RPi4GatewayManager {
public:
    RPi4GatewayManager();
    ~RPi4GatewayManager();
    
    bool StartAll();
    bool StopAll();
    bool RestartGateway(const std::string& gateway_id);
    
    // Configuration
    void SetNumberOfGateways(int count);
    void SetGatewayConfiguration(const std::string& gateway_id, const RPi4Gateway::Config& config);
    void EnableEdgeAnalytics(bool enable);
    
    // Communication interfaces
    bool ConnectToSimulator(const std::string& gateway_id, const std::string& simulator_id);
    bool SetupMQTTForwarding(const std::string& gateway_id, const std::string& mqtt_broker);
    
    // Monitoring
    std::vector<std::string> GetActiveGateways() const;
    RPi4Gateway::SystemStats GetGatewayStats(const std::string& gateway_id) const;
    std::map<std::string, int> GetProcessedMessageCounts() const;
    bool IsGatewayHealthy(const std::string& gateway_id) const;
    
    // Control
    void SetProcessingMode(const std::string& gateway_id, ProcessingMode mode);
    void TriggerDataForwarding(const std::string& gateway_id);
    void InjectProcessingDelay(const std::string& gateway_id, std::chrono::milliseconds delay);
    
    // Test utilities
    void ConfigureForTesting(const TestConfiguration& config);
    void SimulateNetworkLatency(const std::string& gateway_id, double latency_ms);
    
private:
    std::map<std::string, void*> gateways_;  // Using void* to avoid incomplete type issues
    std::map<std::string, std::thread> gateway_threads_;
    std::atomic<bool> running_{false};
    int next_gateway_id_ = 1;
    
    void GatewayThread(const std::string& gateway_id);
    std::string GenerateGatewayId();
};

// MQTT Bridge Manager
class MQTTBridgeManager {
public:
    MQTTBridgeManager();
    ~MQTTBridgeManager();
    
    bool Start();
    bool Stop();
    bool Restart();
    
    // Configuration
    void SetMQTTBroker(const std::string& broker_url, int port);
    void SetWebSocketPort(int port);
    void SetTopicFilters(const std::vector<std::string>& topics);
    void EnableLogging(bool enable);
    
    // Monitoring
    bool IsRunning() const;
    int GetConnectedClients() const;
    std::map<std::string, int> GetTopicMessageCounts() const;
    double getAverageLatency() const;
    
    // Control
    bool PublishTestMessage(const std::string& topic, const std::string& message);
    bool SubscribeToTopic(const std::string& topic);
    void InjectNetworkFailure(std::chrono::seconds duration);
    
    // Test utilities
    void SetupForTesting(const TestConfiguration& config);
    std::vector<std::string> GetReceivedMessages(const std::string& topic) const;
    void ClearMessageHistory();
    
private:
    std::unique_ptr<std::thread> bridge_thread_;
    std::atomic<bool> running_{false};
    std::string mqtt_broker_url_ = "localhost";
    int mqtt_port_ = 1883;
    int websocket_port_ = 8080;
    std::vector<std::string> topic_filters_;
    
    // Message tracking for testing
    mutable std::mutex message_mutex_;
    std::map<std::string, std::vector<std::string>> received_messages_;
    std::map<std::string, int> topic_counts_;
    std::vector<double> latencies_;
    
    void BridgeThread();
    pid_t bridge_process_id_ = -1;
};

// Thermal Monitor Manager
class ThermalMonitorManager {
public:
    ThermalMonitorManager();
    ~ThermalMonitorManager();
    
    bool Start();
    bool Stop();
    bool Restart();
    
    // Configuration
    void SetThermalThresholds(double temp_low, double temp_high, double humidity_high);
    void SetMonitoringInterval(std::chrono::milliseconds interval);
    void SetAlertCooldown(std::chrono::seconds cooldown);
    void EnableAlertTypes(const std::vector<AlertType>& types);
    
    // Monitoring
    bool IsRunning() const;
    std::vector<Alert> GetRecentAlerts(std::chrono::minutes timeframe) const;
    std::map<std::string, SensorData> GetCurrentSensorData() const;
    int GetActiveAlertCount() const;
    
    // Control
    bool ProcessSensorMessage(const std::string& sensor_id, const std::string& message);
    void ClearAlerts();
    void SetSensorOfflineTimeout(std::chrono::seconds timeout);
    
    // Test utilities
    void SetupForTesting(const TestConfiguration& config);
    void InjectTestSensorData(const std::string& sensor_id, double temperature, double humidity);
    void SimulateThresholdViolation(const std::string& sensor_id, AlertType alert_type);
    std::vector<Alert> GetAllGeneratedAlerts() const;
    
private:
    void* thermal_tracker_;  // Using void* to avoid incomplete type issues
    std::unique_ptr<std::thread> monitoring_thread_;
    std::atomic<bool> running_{false};
    
    // Test tracking
    mutable std::mutex alert_mutex_;
    std::vector<Alert> generated_alerts_;
    
    void MonitoringThread();
    void OnAlertGenerated(const Alert& alert);
};

// Integration utilities
namespace IntegrationUtils {
    // Component health checking
    bool CheckComponentHealth(const std::string& component_name);
    std::map<std::string, bool> CheckAllComponentsHealth();
    
    // Process management
    pid_t StartProcess(const std::string& command, const std::vector<std::string>& args);
    bool StopProcess(pid_t pid, std::chrono::seconds timeout = std::chrono::seconds(10));
    bool IsProcessRunning(pid_t pid);
    
    // Network utilities
    bool IsPortOpen(int port);
    bool WaitForPortOpen(int port, std::chrono::seconds timeout);
    std::string GetLocalIPAddress();
    
    // File and logging utilities
    std::string ReadLogFile(const std::string& log_file, int last_n_lines = -1);
    void ClearLogFile(const std::string& log_file);
    bool WaitForLogMessage(const std::string& log_file, const std::string& message, 
                          std::chrono::seconds timeout);
    
    // Performance monitoring
    struct SystemMetrics {
        double cpu_usage_percent;
        size_t memory_usage_kb;
        size_t disk_usage_kb;
        int network_connections;
        std::map<std::string, double> custom_metrics;
    };
    
    SystemMetrics GetSystemMetrics();
    void StartPerformanceMonitoring();
    void StopPerformanceMonitoring();
    std::vector<SystemMetrics> GetPerformanceHistory();
    
    // Test data generation
    std::string GenerateRealisticSensorMessage(const std::string& sensor_id, 
                                               double base_temp = 22.0, 
                                               double base_humidity = 45.0);
    std::vector<std::string> GenerateTestSensorIds(int count);
    std::string GenerateRandomLocation();
    
    // Message validation
    bool ValidateJSONMessage(const std::string& message);
    bool ValidateSensorDataMessage(const std::string& message);
    bool ValidateAlertMessage(const std::string& message);
    
    // Timing utilities
    void PreciseSleep(std::chrono::microseconds duration);
    std::chrono::steady_clock::time_point GetHighPrecisionTime();
    double CalculatePreciseLatency(const std::chrono::steady_clock::time_point& start,
                                  const std::chrono::steady_clock::time_point& end);
}

#endif // COMPONENT_MANAGERS_H 