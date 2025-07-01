#ifndef MQTT_ONLY_BRIDGE_SIMPLE_H
#define MQTT_ONLY_BRIDGE_SIMPLE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <queue>
#include <mosquitto.h>
#include <jsoncpp/json/json.h>

// Forward declarations for thermal monitoring integration
namespace thermal_monitoring {
    struct ThermalConfig;
    struct Alert;
    class ThermalIsolationTracker;
}

/**
 * MQTT-Only Bridge - Simplified Pure MQTT Implementation
 * 
 * Optimized for high-throughput IoT communication
 */
class MQTTOnlyBridge {
public:
    // Configuration structure
    struct Config {
        std::string broker_host;
        int broker_port;
        std::string client_id;
        int keepalive;
        bool clean_session;
        
        // QoS settings
        int default_qos;
        int sensor_data_qos;
        int alerts_qos;
        int commands_qos;
        
        // Performance settings
        int max_inflight_messages;
        int message_retry_count;
        std::chrono::milliseconds reconnect_delay;
        std::chrono::milliseconds publish_timeout;
        
        // Topic configuration
        std::string sensor_topic_prefix;
        std::string alert_topic_prefix;
        std::string command_topic_prefix;
        std::string status_topic_prefix;
        
        // Processing settings
        bool enable_thermal_processing;
        bool enable_data_aggregation;
        bool enable_command_processing;
        size_t message_buffer_size;
        int worker_thread_count;
        
        // Default constructor
        Config() : 
            broker_host("localhost"),
            broker_port(1883),
            client_id("mqtt_only_bridge"),
            keepalive(60),
            clean_session(true),
            default_qos(1),
            sensor_data_qos(0),
            alerts_qos(2),
            commands_qos(1),
            max_inflight_messages(1000),
            message_retry_count(3),
            reconnect_delay(5000),
            publish_timeout(10000),
            sensor_topic_prefix("sensors/"),
            alert_topic_prefix("alerts/"),
            command_topic_prefix("commands/"),
            status_topic_prefix("status/"),
            enable_thermal_processing(true),
            enable_data_aggregation(true),
            enable_command_processing(true),
            message_buffer_size(10000),
            worker_thread_count(4) {}
    };
    
    // Message types for internal processing
    enum class MessageType {
        SENSOR_DATA,
        ALERT,
        COMMAND,
        STATUS,
        THERMAL_DATA,
        SYSTEM_MESSAGE
    };
    
    // Internal message structure
    struct InternalMessage {
        MessageType type;
        std::string topic;
        std::string payload;
        int qos;
        std::chrono::steady_clock::time_point timestamp;
        std::string sender_id;
        int retry_count;
        
        InternalMessage() : type(MessageType::SYSTEM_MESSAGE), qos(0), retry_count(0) {}
    };
    
    // Performance metrics (simplified to avoid atomic copy issues)
    struct Metrics {
        uint64_t messages_received;
        uint64_t messages_published;
        uint64_t messages_processed;
        uint64_t messages_failed;
        uint64_t thermal_alerts_generated;
        uint64_t commands_executed;
        
        double avg_processing_time_ms;
        double max_processing_time_ms;
        size_t current_queue_size;
        size_t max_queue_size;
        uint64_t total_bytes_received;
        uint64_t total_bytes_sent;
        
        std::chrono::steady_clock::time_point start_time;
        
        // Connection metrics
        uint32_t connection_count;
        uint32_t reconnection_count;
        bool is_connected;
        
        Metrics() : 
            messages_received(0), messages_published(0), messages_processed(0), 
            messages_failed(0), thermal_alerts_generated(0), commands_executed(0),
            avg_processing_time_ms(0.0), max_processing_time_ms(0.0), 
            current_queue_size(0), max_queue_size(0), total_bytes_received(0), 
            total_bytes_sent(0), connection_count(0), reconnection_count(0), 
            is_connected(false) {
            start_time = std::chrono::steady_clock::now();
        }
    };
    
    // Connection status
    enum class ConnectionStatus {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTING,
        ERROR
    };
    
    // Callback function types
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload, int qos)>;
    using AlertCallback = std::function<void(const thermal_monitoring::Alert& alert)>;
    using CommandCallback = std::function<std::string(const std::string& command, const Json::Value& params)>;
    using StatusCallback = std::function<void(const std::string& status)>;
    using ConnectionCallback = std::function<void(ConnectionStatus status, const std::string& message)>;

public:
    explicit MQTTOnlyBridge(const Config& config = Config());
    ~MQTTOnlyBridge();
    
    // Core operations
    bool Start();
    bool Stop();
    bool Restart();
    bool IsRunning() const;
    
    // Publishing methods
    bool PublishSensorData(const std::string& sensor_id, const Json::Value& data);
    bool PublishAlert(const thermal_monitoring::Alert& alert);
    bool PublishCommand(const std::string& target, const std::string& command, const Json::Value& params);
    bool PublishStatus(const std::string& component, const std::string& status);
    bool PublishMessage(const std::string& topic, const std::string& payload, int qos = -1, bool retain = false);
    
    // Subscription management
    bool Subscribe(const std::string& topic, int qos = -1);
    bool Unsubscribe(const std::string& topic);
    bool SubscribeToSensorData(const std::string& sensor_pattern = "+");
    bool SubscribeToAlerts(const std::string& alert_pattern = "+");
    bool SubscribeToCommands(const std::string& command_pattern = "+");
    
    // Callback registration
    void SetMessageCallback(MessageCallback callback);
    void SetAlertCallback(AlertCallback callback);
    void SetCommandCallback(CommandCallback callback);
    void SetStatusCallback(StatusCallback callback);
    void SetConnectionCallback(ConnectionCallback callback);
    
    // Thermal monitoring integration
    bool EnableThermalMonitoring(const thermal_monitoring::ThermalConfig& thermal_config);
    void DisableThermalMonitoring();
    bool ProcessThermalData(const std::string& sensor_id, double temperature, double humidity);
    
    // Configuration and status
    void UpdateConfig(const Config& new_config);
    Config GetConfig() const;
    Metrics GetMetrics() const;
    std::vector<std::string> GetActiveSubscriptions() const;
    ConnectionStatus GetConnectionStatus() const;
    
    // Performance and monitoring
    void ResetMetrics();
    double GetCurrentThroughput() const;
    double GetAverageLatency() const;
    size_t GetQueueSize() const;
    bool IsConnected() const;
    std::string GetConnectionInfo() const;

private:
    // Core components
    Config config_;
    struct mosquitto* mosq_;
    std::atomic<bool> running_;
    std::atomic<ConnectionStatus> connection_status_;
    
    // Threading
    std::unique_ptr<std::thread> main_thread_;
    std::vector<std::unique_ptr<std::thread>> worker_threads_;
    std::unique_ptr<std::thread> metrics_thread_;
    
    // Message processing
    std::queue<InternalMessage> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> stop_processing_;
    
    // Callbacks
    MessageCallback message_callback_;
    AlertCallback alert_callback_;
    CommandCallback command_callback_;
    StatusCallback status_callback_;
    ConnectionCallback connection_callback_;
    
    // Metrics and monitoring (using atomics for thread safety)
    std::atomic<uint64_t> messages_received_;
    std::atomic<uint64_t> messages_published_;
    std::atomic<uint64_t> messages_processed_;
    std::atomic<uint64_t> messages_failed_;
    std::atomic<uint64_t> thermal_alerts_generated_;
    std::atomic<uint64_t> commands_executed_;
    std::atomic<double> avg_processing_time_ms_;
    std::atomic<double> max_processing_time_ms_;
    std::atomic<size_t> current_queue_size_;
    std::atomic<size_t> max_queue_size_;
    std::atomic<bool> is_connected_;
    std::chrono::steady_clock::time_point start_time_;
    mutable std::mutex metrics_mutex_;
    
    // Thermal monitoring
    std::atomic<bool> thermal_enabled_;
    std::unique_ptr<thermal_monitoring::ThermalIsolationTracker> thermal_tracker_;
    std::map<std::string, std::chrono::steady_clock::time_point> last_sensor_update_;
    std::mutex thermal_mutex_;
    
    // Subscriptions
    std::vector<std::string> active_subscriptions_;
    std::mutex subscriptions_mutex_;
    
    // Internal methods
    void MainLoop();
    void WorkerLoop();
    void MetricsLoop();
    
    // MQTT callbacks (static)
    static void OnConnect(struct mosquitto* mosq, void* userdata, int result);
    static void OnDisconnect(struct mosquitto* mosq, void* userdata, int result);
    static void OnMessage(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* msg);
    static void OnPublish(struct mosquitto* mosq, void* userdata, int mid);
    static void OnSubscribe(struct mosquitto* mosq, void* userdata, int mid, int qos_count, const int* granted_qos);
    static void OnLog(struct mosquitto* mosq, void* userdata, int level, const char* str);
    
    // Message processing
    void ProcessMessage(const InternalMessage& msg);
    void ProcessSensorData(const std::string& topic, const std::string& payload);
    void ProcessAlertMessage(const std::string& topic, const std::string& payload);
    void ProcessCommand(const std::string& topic, const std::string& payload);
    void ProcessStatusMessage(const std::string& topic, const std::string& payload);
    
    // Thermal processing
    void CheckThermalThresholds(const std::string& sensor_id, double temperature, double humidity);
    void GenerateThermalAlert(const std::string& sensor_id, const std::string& alert_type, 
                             double value, const std::string& threshold_info);
    
    // Utility methods
    MessageType DetermineMessageType(const std::string& topic);
    std::string ExtractSensorId(const std::string& topic);
    bool ParseSensorData(const std::string& payload, double& temperature, double& humidity);
    Json::Value CreateAlertMessage(const std::string& sensor_id, const std::string& alert_type, 
                                  const std::string& message);
    
    // Performance monitoring
    void UpdateLatencyMetrics(double latency_ms);
    void UpdateProcessingMetrics(double processing_time_ms);
    
    // Connection management
    bool InitializeMosquitto();
    void CleanupMosquitto();
    bool ConnectToBroker();
    void HandleReconnection();
    void UpdateConnectionStatus(ConnectionStatus status, const std::string& message = "");
    
    // Error handling and logging
    void LogError(const std::string& message);
    void LogInfo(const std::string& message);
    void LogDebug(const std::string& message);
    void LogWarning(const std::string& message);
};

// Utility functions for MQTT-only operations
namespace MQTTOnlyUtils {
    // Topic utilities
    std::string BuildSensorTopic(const std::string& prefix, const std::string& sensor_id, const std::string& data_type = "data");
    std::string BuildAlertTopic(const std::string& prefix, const std::string& alert_type);
    std::string BuildCommandTopic(const std::string& prefix, const std::string& target);
    std::string BuildStatusTopic(const std::string& prefix, const std::string& component);
    
    // Message utilities
    std::string CreateSensorDataPayload(const std::string& sensor_id, double temperature, 
                                       double humidity, const std::string& location = "");
    std::string CreateAlertPayload(const std::string& sensor_id, const std::string& alert_type,
                                  const std::string& message, double value = 0.0);
    std::string CreateCommandPayload(const std::string& command, const Json::Value& parameters);
    std::string CreateStatusPayload(const std::string& component, const std::string& status,
                                   const Json::Value& details = Json::Value{});
    
    // Parsing utilities
    bool ParseSensorDataPayload(const std::string& payload, std::string& sensor_id, 
                               double& temperature, double& humidity, std::string& location);
    bool ParseAlertPayload(const std::string& payload, std::string& sensor_id, 
                          std::string& alert_type, std::string& message, double& value);
    bool ParseCommandPayload(const std::string& payload, std::string& command, Json::Value& parameters);
    
    // Performance utilities
    double CalculateThroughput(uint64_t message_count, std::chrono::milliseconds duration);
    std::string FormatMetrics(const MQTTOnlyBridge::Metrics& metrics);
    
    // Configuration utilities
    MQTTOnlyBridge::Config CreateDefaultConfig();
    MQTTOnlyBridge::Config CreateHighThroughputConfig();
    MQTTOnlyBridge::Config CreateLowLatencyConfig();
    
    // Testing utilities
    bool ValidateBrokerConnection(const std::string& host, int port, std::chrono::seconds timeout = std::chrono::seconds(5));
    std::vector<std::string> GenerateTestSensorData(int sensor_count, int message_count);
    void RunPerformanceTest(MQTTOnlyBridge& bridge, int duration_seconds, int messages_per_second);
}

#endif // MQTT_ONLY_BRIDGE_SIMPLE_H
