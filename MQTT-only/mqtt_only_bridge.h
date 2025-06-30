#ifndef MQTT_ONLY_BRIDGE_H
#define MQTT_ONLY_BRIDGE_H

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
 * MQTT-Only Bridge - Pure MQTT Implementation
 * 
 * Optimized for:
 * - IoT device-to-device communication
 * - Server-side data processing
 * - High-throughput sensor data ingestion
 * - Low-latency command distribution
 * - Minimal memory footprint
 * - MQTT v3.1.1 and v5.0 protocol support
 */
class MQTTOnlyBridge {
public:
    // Configuration structure
    struct Config {
        std::string broker_host = "localhost";
        int broker_port = 1883;
        std::string client_id = "mqtt_only_bridge";
        int keepalive = 60;
        bool clean_session = true;
        
        // Authentication
        std::string username = "";
        std::string password = "";
        
        // TLS/SSL configuration
        bool use_tls = false;
        std::string ca_cert_path = "";
        std::string client_cert_path = "";
        std::string client_key_path = "";
        
        // QoS settings
        int default_qos = 1;
        int sensor_data_qos = 0;    // Fire-and-forget for high-frequency data
        int alerts_qos = 2;         // Exactly-once for critical alerts
        int commands_qos = 1;       // At-least-once for commands
        
        // Performance settings
        int max_inflight_messages = 1000;
        int message_retry_count = 3;
        std::chrono::milliseconds reconnect_delay{5000};
        std::chrono::milliseconds publish_timeout{10000};
        
        // Topic configuration
        std::string sensor_topic_prefix = "sensors/";
        std::string alert_topic_prefix = "alerts/";
        std::string command_topic_prefix = "commands/";
        std::string status_topic_prefix = "status/";
        std::string response_topic_prefix = "responses/";
        
        // Processing settings
        bool enable_thermal_processing = true;
        bool enable_data_aggregation = true;
        bool enable_command_processing = true;
        bool enable_message_persistence = false;
        size_t message_buffer_size = 10000;
        int worker_thread_count = 4;
        
        // Message routing
        bool enable_topic_filtering = true;
        std::vector<std::string> allowed_topics;
        std::vector<std::string> blocked_topics;
    };
    
    // Message types for internal processing
    enum class MessageType {
        SENSOR_DATA,
        ALERT,
        COMMAND,
        STATUS,
        THERMAL_DATA,
        SYSTEM_MESSAGE,
        RESPONSE,
        HEARTBEAT
    };
    
    // Internal message structure
    struct InternalMessage {
        MessageType type;
        std::string topic;
        std::string payload;
        int qos;
        std::chrono::steady_clock::time_point timestamp;
        std::string sender_id;
        int retry_count = 0;
        bool retain = false;
        uint32_t message_id = 0;
    };
    
    // Performance metrics
    struct Metrics {
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> messages_published{0};
        std::atomic<uint64_t> messages_processed{0};
        std::atomic<uint64_t> messages_failed{0};
        std::atomic<uint64_t> thermal_alerts_generated{0};
        std::atomic<uint64_t> commands_executed{0};
        std::atomic<uint64_t> responses_sent{0};
        std::atomic<uint64_t> heartbeats_processed{0};
        
        std::atomic<double> avg_processing_time_ms{0.0};
        std::atomic<double> max_processing_time_ms{0.0};
        std::atomic<size_t> current_queue_size{0};
        std::atomic<size_t> max_queue_size{0};
        std::atomic<uint64_t> total_bytes_received{0};
        std::atomic<uint64_t> total_bytes_sent{0};
        
        std::chrono::steady_clock::time_point start_time;
        mutable std::mutex latency_mutex;
        std::vector<double> recent_latencies;
        
        // Connection metrics
        std::atomic<uint32_t> connection_count{0};
        std::atomic<uint32_t> reconnection_count{0};
        std::atomic<bool> is_connected{false};
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
    explicit MQTTOnlyBridge(const Config& config = Config{});
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
    bool PublishResponse(const std::string& request_id, const std::string& response);
    bool PublishMessage(const std::string& topic, const std::string& payload, int qos = -1, bool retain = false);
    bool PublishHeartbeat(const std::string& client_id);
    
    // Subscription management
    bool Subscribe(const std::string& topic, int qos = -1);
    bool Unsubscribe(const std::string& topic);
    bool SubscribeToSensorData(const std::string& sensor_pattern = "+");
    bool SubscribeToAlerts(const std::string& alert_pattern = "+");
    bool SubscribeToCommands(const std::string& command_pattern = "+");
    bool SubscribeToStatus(const std::string& status_pattern = "+");
    bool SubscribeToResponses(const std::string& response_pattern = "+");
    
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
    
    // Advanced MQTT features
    bool SetRetainedMessage(const std::string& topic, const std::string& payload);
    bool ClearRetainedMessage(const std::string& topic);
    void SetLastWillTestament(const std::string& topic, const std::string& payload, int qos = 1, bool retain = false);
    
    // Message persistence (optional)
    bool EnableMessagePersistence(const std::string& persistence_file);
    void DisableMessagePersistence();
    
    // Performance and monitoring
    void ResetMetrics();
    double GetCurrentThroughput() const;
    double GetAverageLatency() const;
    size_t GetQueueSize() const;
    bool IsConnected() const;
    std::string GetConnectionInfo() const;
    
    // Topic filtering and routing
    bool AddTopicFilter(const std::string& pattern, bool allow = true);
    bool RemoveTopicFilter(const std::string& pattern);
    std::vector<std::string> GetTopicFilters() const;
    
    // Client management (for broker mode)
    struct ClientInfo {
        std::string client_id;
        std::string address;
        std::chrono::steady_clock::time_point connected_time;
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
    };
    
    std::vector<ClientInfo> GetConnectedClients() const;

private:
    // Core components
    Config config_;
    struct mosquitto* mosq_;
    std::atomic<bool> running_{false};
    std::atomic<ConnectionStatus> connection_status_{ConnectionStatus::DISCONNECTED};
    
    // Threading
    std::unique_ptr<std::thread> main_thread_;
    std::vector<std::unique_ptr<std::thread>> worker_threads_;
    std::unique_ptr<std::thread> metrics_thread_;
    std::unique_ptr<std::thread> heartbeat_thread_;
    
    // Message processing
    std::queue<InternalMessage> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> stop_processing_{false};
    
    // Callbacks
    MessageCallback message_callback_;
    AlertCallback alert_callback_;
    CommandCallback command_callback_;
    StatusCallback status_callback_;
    ConnectionCallback connection_callback_;
    
    // Metrics and monitoring
    mutable Metrics metrics_;
    mutable std::mutex metrics_mutex_;
    
    // Thermal monitoring
    std::atomic<bool> thermal_enabled_{false};
    std::unique_ptr<thermal_monitoring::ThermalIsolationTracker> thermal_tracker_;
    std::map<std::string, std::chrono::steady_clock::time_point> last_sensor_update_;
    std::mutex thermal_mutex_;
    
    // Subscriptions
    std::vector<std::string> active_subscriptions_;
    std::mutex subscriptions_mutex_;
    
    // Topic filtering
    std::vector<std::string> allowed_topic_patterns_;
    std::vector<std::string> blocked_topic_patterns_;
    std::mutex topic_filter_mutex_;
    
    // Message persistence
    std::atomic<bool> persistence_enabled_{false};
    std::string persistence_file_;
    std::mutex persistence_mutex_;
    
    // Client tracking (for broker mode)
    std::map<std::string, ClientInfo> connected_clients_;
    std::mutex clients_mutex_;
    
    // Internal methods
    void MainLoop();
    void WorkerLoop();
    void MetricsLoop();
    void HeartbeatLoop();
    
    // MQTT callbacks (static)
    static void OnConnect(struct mosquitto* mosq, void* userdata, int result);
    static void OnDisconnect(struct mosquitto* mosq, void* userdata, int result);
    static void OnMessage(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* msg);
    static void OnPublish(struct mosquitto* mosq, void* userdata, int mid);
    static void OnSubscribe(struct mosquitto* mosq, void* userdata, int mid, int qos_count, const int* granted_qos);
    static void OnUnsubscribe(struct mosquitto* mosq, void* userdata, int mid);
    static void OnLog(struct mosquitto* mosq, void* userdata, int level, const char* str);
    
    // Message processing
    void ProcessMessage(const InternalMessage& msg);
    void ProcessSensorData(const std::string& topic, const std::string& payload);
    void ProcessAlertMessage(const std::string& topic, const std::string& payload);
    void ProcessCommand(const std::string& topic, const std::string& payload);
    void ProcessStatusMessage(const std::string& topic, const std::string& payload);
    void ProcessResponse(const std::string& topic, const std::string& payload);
    void ProcessHeartbeat(const std::string& topic, const std::string& payload);
    
    // Thermal processing
    void CheckThermalThresholds(const std::string& sensor_id, double temperature, double humidity);
    void GenerateThermalAlert(const std::string& sensor_id, const std::string& alert_type, 
                             double value, const std::string& threshold_info);
    
    // Utility methods
    MessageType DetermineMessageType(const std::string& topic);
    std::string ExtractSensorId(const std::string& topic);
    std::string ExtractClientId(const std::string& topic);
    bool ParseSensorData(const std::string& payload, double& temperature, double& humidity);
    Json::Value CreateAlertMessage(const std::string& sensor_id, const std::string& alert_type, 
                                  const std::string& message);
    
    // Topic filtering
    bool IsTopicAllowed(const std::string& topic);
    bool MatchesPattern(const std::string& topic, const std::string& pattern);
    
    // Performance monitoring
    void UpdateLatencyMetrics(double latency_ms);
    void UpdateProcessingMetrics(double processing_time_ms);
    void UpdateThroughputMetrics(size_t bytes_count, bool is_received = true);
    
    // Connection management
    bool InitializeMosquitto();
    void CleanupMosquitto();
    bool ConnectToBroker();
    void HandleReconnection();
    void UpdateConnectionStatus(ConnectionStatus status, const std::string& message = "");
    
    // Message persistence
    bool SaveMessageToDisk(const InternalMessage& msg);
    std::vector<InternalMessage> LoadMessagesFromDisk();
    void CleanupPersistenceFile();
    
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
    std::string BuildResponseTopic(const std::string& prefix, const std::string& request_id);
    std::string BuildHeartbeatTopic(const std::string& prefix, const std::string& client_id);
    
    // Message utilities
    std::string CreateSensorDataPayload(const std::string& sensor_id, double temperature, 
                                       double humidity, const std::string& location = "");
    std::string CreateAlertPayload(const std::string& sensor_id, const std::string& alert_type,
                                  const std::string& message, double value = 0.0);
    std::string CreateCommandPayload(const std::string& command, const Json::Value& parameters);
    std::string CreateStatusPayload(const std::string& component, const std::string& status,
                                   const Json::Value& details = Json::Value{});
    std::string CreateResponsePayload(const std::string& request_id, const std::string& response,
                                     bool success = true);
    std::string CreateHeartbeatPayload(const std::string& client_id, const Json::Value& status = Json::Value{});
    
    // Parsing utilities
    bool ParseSensorDataPayload(const std::string& payload, std::string& sensor_id, 
                               double& temperature, double& humidity, std::string& location);
    bool ParseAlertPayload(const std::string& payload, std::string& sensor_id, 
                          std::string& alert_type, std::string& message, double& value);
    bool ParseCommandPayload(const std::string& payload, std::string& command, Json::Value& parameters);
    bool ParseResponsePayload(const std::string& payload, std::string& request_id, 
                             std::string& response, bool& success);
    bool ParseHeartbeatPayload(const std::string& payload, std::string& client_id, Json::Value& status);
    
    // Performance utilities
    double CalculateThroughput(uint64_t message_count, std::chrono::milliseconds duration);
    double CalculateBandwidth(uint64_t bytes_count, std::chrono::milliseconds duration);
    std::string FormatMetrics(const MQTTOnlyBridge::Metrics& metrics);
    std::string FormatLatencyStats(const std::vector<double>& latencies);
    
    // Configuration utilities
    MQTTOnlyBridge::Config LoadConfigFromFile(const std::string& config_file);
    bool SaveConfigToFile(const MQTTOnlyBridge::Config& config, const std::string& config_file);
    MQTTOnlyBridge::Config CreateDefaultConfig();
    MQTTOnlyBridge::Config CreateHighThroughputConfig();
    MQTTOnlyBridge::Config CreateLowLatencyConfig();
    
    // Testing utilities
    bool ValidateBrokerConnection(const std::string& host, int port, std::chrono::seconds timeout = std::chrono::seconds(5));
    std::vector<std::string> GenerateTestSensorData(int sensor_count, int message_count);
    void RunPerformanceTest(MQTTOnlyBridge& bridge, int duration_seconds, int messages_per_second);
    void RunStressTest(MQTTOnlyBridge& bridge, int concurrent_clients, int duration_seconds);
    void RunLatencyTest(MQTTOnlyBridge& bridge, int message_count, int interval_ms);
    
    // Validation utilities
    bool ValidateTopicName(const std::string& topic);
    bool ValidateQoSLevel(int qos);
    bool ValidatePayloadSize(const std::string& payload, size_t max_size = 268435455);
    bool ValidateClientId(const std::string& client_id);
    
    // Conversion utilities
    std::string ConnectionStatusToString(MQTTOnlyBridge::ConnectionStatus status);
    std::string MessageTypeToString(MQTTOnlyBridge::MessageType type);
    MQTTOnlyBridge::MessageType StringToMessageType(const std::string& type_str);
}

#endif // MQTT_ONLY_BRIDGE_H
