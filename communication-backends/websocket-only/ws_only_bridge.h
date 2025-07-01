#ifndef WS_ONLY_BRIDGE_H
#define WS_ONLY_BRIDGE_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <queue>
#include <libwebsockets.h>
#include <jsoncpp/json/json.h>

// Forward declarations for thermal monitoring integration
namespace thermal_monitoring {
    struct ThermalConfig;
    struct Alert;
    class ThermalIsolationTracker;
}

/**
 * WebSocket-Only Bridge - Pure WebSocket Implementation
 * 
 * Optimized for:
 * - Real-time web applications
 * - Browser-based IoT dashboards
 * - Live sensor data streaming
 * - Instant messaging and notifications
 * - Room-based communication channels
 * - Zero-latency client-to-client communication
 */
class WebSocketOnlyBridge {
public:
    // Configuration structure
    struct Config {
        std::string host;
        int port;
        std::string ssl_cert_path;
        std::string ssl_key_path;
        bool use_ssl;
        
        // Server settings
        int max_connections;
        int max_frame_size;
        int ping_interval_seconds;
        int pong_timeout_seconds;
        
        // Performance settings
        size_t message_buffer_size;
        int worker_thread_count;
        bool enable_compression;
        bool enable_per_message_deflate;
        
        // Room settings
        size_t max_rooms;
        size_t max_room_size;
        std::string default_room;
        bool auto_create_rooms;
        
        // Processing settings
        bool enable_thermal_processing;
        bool enable_message_logging;
        bool enable_heartbeat;
        bool enable_binary_frames;
        
        // Security settings
        std::vector<std::string> allowed_origins;
        bool enable_origin_check;
        size_t max_message_size;
        int rate_limit_messages_per_second;
        
        // Default constructor
        Config() : 
            host("0.0.0.0"),
            port(8080),
            ssl_cert_path(""),
            ssl_key_path(""),
            use_ssl(false),
            max_connections(1000),
            max_frame_size(65536),
            ping_interval_seconds(30),
            pong_timeout_seconds(10),
            message_buffer_size(10000),
            worker_thread_count(4),
            enable_compression(true),
            enable_per_message_deflate(true),
            max_rooms(100),
            max_room_size(500),
            default_room("general"),
            auto_create_rooms(true),
            enable_thermal_processing(true),
            enable_message_logging(false),
            enable_heartbeat(true),
            enable_binary_frames(false),
            enable_origin_check(false),
            max_message_size(1048576),
            rate_limit_messages_per_second(100) {}
    };
    
    // Message types for WebSocket communication
    enum class MessageType {
        SENSOR_DATA,
        ALERT,
        COMMAND,
        STATUS,
        THERMAL_DATA,
        ROOM_JOIN,
        ROOM_LEAVE,
        ROOM_MESSAGE,
        PRIVATE_MESSAGE,
        BROADCAST,
        HEARTBEAT,
        SYSTEM_MESSAGE
    };
    
    // Connection information
    struct ConnectionInfo {
        struct lws* wsi;
        std::string client_id;
        std::string room;
        std::string user_agent;
        std::string origin;
        std::chrono::steady_clock::time_point connected_time;
        std::chrono::steady_clock::time_point last_activity;
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        bool is_authenticated;
        
        ConnectionInfo() : wsi(nullptr), is_authenticated(false) {
            connected_time = std::chrono::steady_clock::now();
            last_activity = connected_time;
        }
    };
    
    // Room information
    struct RoomInfo {
        std::string name;
        std::unordered_set<std::string> clients;
        std::chrono::steady_clock::time_point created_time;
        std::atomic<uint64_t> message_count{0};
        std::atomic<uint64_t> total_bytes{0};
        std::string description;
        bool is_private;
        size_t max_clients;
        
        RoomInfo(const std::string& room_name) : 
            name(room_name), is_private(false), max_clients(500) {
            created_time = std::chrono::steady_clock::now();
        }
    };
    
    // Internal message structure
    struct InternalMessage {
        MessageType type;
        std::string from_client;
        std::string to_client;
        std::string room;
        std::string payload;
        std::chrono::steady_clock::time_point timestamp;
        bool is_binary;
        int priority;
        
        InternalMessage() : type(MessageType::SYSTEM_MESSAGE), is_binary(false), priority(0) {
            timestamp = std::chrono::steady_clock::now();
        }
    };
    
    // Performance metrics
    struct Metrics {
        std::atomic<uint64_t> connections_total{0};
        std::atomic<uint64_t> connections_current{0};
        std::atomic<uint64_t> connections_peak{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_broadcast{0};
        std::atomic<uint64_t> messages_failed{0};
        std::atomic<uint64_t> thermal_alerts_generated{0};
        std::atomic<uint64_t> rooms_created{0};
        std::atomic<uint64_t> rooms_active{0};
        
        std::atomic<double> avg_processing_time_ms{0.0};
        std::atomic<double> max_processing_time_ms{0.0};
        std::atomic<size_t> current_queue_size{0};
        std::atomic<size_t> max_queue_size{0};
        std::atomic<uint64_t> total_bytes_sent{0};
        std::atomic<uint64_t> total_bytes_received{0};
        
        std::chrono::steady_clock::time_point start_time;
        mutable std::mutex latency_mutex;
        std::vector<double> recent_latencies;
        
        Metrics() {
            start_time = std::chrono::steady_clock::now();
        }
    };
    
    // Connection status
    enum class ServerStatus {
        STOPPED,
        STARTING,
        RUNNING,
        STOPPING,
        ERROR
    };
    
    // Callback function types
    using MessageCallback = std::function<void(const std::string& client_id, const std::string& message, const std::string& room)>;
    using ConnectionCallback = std::function<void(const std::string& client_id, bool connected)>;
    using AlertCallback = std::function<void(const thermal_monitoring::Alert& alert)>;
    using RoomCallback = std::function<void(const std::string& room, const std::string& client_id, bool joined)>;
    using StatusCallback = std::function<void(ServerStatus status, const std::string& message)>;

public:
    explicit WebSocketOnlyBridge(const Config& config = Config());
    ~WebSocketOnlyBridge();
    
    // Core operations
    bool Start();
    bool Stop();
    bool Restart();
    bool IsRunning() const;
    
    // Message operations
    bool SendMessage(const std::string& client_id, const std::string& message, bool is_binary = false);
    bool SendToRoom(const std::string& room, const std::string& message, bool is_binary = false);
    bool BroadcastMessage(const std::string& message, bool is_binary = false);
    bool SendSensorData(const std::string& room, const std::string& sensor_id, const Json::Value& data);
    bool SendAlert(const std::string& room, const thermal_monitoring::Alert& alert);
    bool SendCommand(const std::string& client_id, const std::string& command, const Json::Value& params);
    bool SendStatus(const std::string& room, const std::string& component, const std::string& status);
    
    // Room management
    bool CreateRoom(const std::string& room_name, const std::string& description = "", bool is_private = false);
    bool DeleteRoom(const std::string& room_name);
    bool JoinRoom(const std::string& client_id, const std::string& room_name);
    bool LeaveRoom(const std::string& client_id, const std::string& room_name);
    std::vector<std::string> GetRooms() const;
    std::vector<std::string> GetRoomClients(const std::string& room_name) const;
    RoomInfo GetRoomInfo(const std::string& room_name) const;
    
    // Connection management
    std::vector<std::string> GetConnectedClients() const;
    ConnectionInfo GetConnectionInfo(const std::string& client_id) const;
    bool DisconnectClient(const std::string& client_id, const std::string& reason = "");
    size_t GetConnectionCount() const;
    
    // Callback registration
    void SetMessageCallback(MessageCallback callback);
    void SetConnectionCallback(ConnectionCallback callback);
    void SetAlertCallback(AlertCallback callback);
    void SetRoomCallback(RoomCallback callback);
    void SetStatusCallback(StatusCallback callback);
    
    // Thermal monitoring integration
    bool EnableThermalMonitoring(const thermal_monitoring::ThermalConfig& thermal_config);
    void DisableThermalMonitoring();
    bool ProcessThermalData(const std::string& sensor_id, double temperature, double humidity);
    
    // Configuration and status
    void UpdateConfig(const Config& new_config);
    Config GetConfig() const;
    Metrics GetMetrics() const;
    ServerStatus GetServerStatus() const;
    
    // Performance and monitoring
    void ResetMetrics();
    double GetCurrentThroughput() const;
    double GetAverageLatency() const;
    size_t GetQueueSize() const;
    std::string GetServerInfo() const;

private:
    // Core components
    Config config_;
    struct lws_context* lws_context_;
    std::atomic<bool> running_;
    std::atomic<ServerStatus> server_status_;
    
    // Threading
    std::unique_ptr<std::thread> server_thread_;
    std::vector<std::unique_ptr<std::thread>> worker_threads_;
    std::unique_ptr<std::thread> metrics_thread_;
    std::unique_ptr<std::thread> heartbeat_thread_;
    
    // Message processing
    std::queue<InternalMessage> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> stop_processing_;
    
    // Connection management
    std::unordered_map<std::string, std::unique_ptr<ConnectionInfo>> connections_;
    std::unordered_map<struct lws*, std::string> wsi_to_client_;
    std::mutex connections_mutex_;
    
    // Room management
    std::unordered_map<std::string, std::unique_ptr<RoomInfo>> rooms_;
    std::mutex rooms_mutex_;
    
    // Callbacks
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    AlertCallback alert_callback_;
    RoomCallback room_callback_;
    StatusCallback status_callback_;
    
    // Metrics and monitoring
    mutable Metrics metrics_;
    mutable std::mutex metrics_mutex_;
    
    // Thermal monitoring
    std::atomic<bool> thermal_enabled_;
    std::unique_ptr<thermal_monitoring::ThermalIsolationTracker> thermal_tracker_;
    std::map<std::string, std::chrono::steady_clock::time_point> last_sensor_update_;
    std::mutex thermal_mutex_;
    
    // LibWebSockets protocol data
    struct PerSessionData {
        std::string client_id;
        size_t message_count;
        std::chrono::steady_clock::time_point last_activity;
    };
    
    // Internal methods
    void ServerLoop();
    void WorkerLoop();
    void MetricsLoop();
    void HeartbeatLoop();
    
    // LibWebSockets callbacks (static)
    static int WebSocketCallback(struct lws* wsi, enum lws_callback_reasons reason,
                               void* user, void* in, size_t len);
    static int HttpCallback(struct lws* wsi, enum lws_callback_reasons reason,
                          void* user, void* in, size_t len);
    
    // Message processing
    void ProcessMessage(const InternalMessage& msg);
    void ProcessSensorData(const std::string& from_client, const std::string& room, const std::string& payload);
    void ProcessAlertMessage(const std::string& from_client, const std::string& room, const std::string& payload);
    void ProcessCommand(const std::string& from_client, const std::string& to_client, const std::string& payload);
    void ProcessRoomMessage(const std::string& from_client, const std::string& room, const std::string& payload);
    void ProcessPrivateMessage(const std::string& from_client, const std::string& to_client, const std::string& payload);
    
    // Connection handling
    void HandleNewConnection(struct lws* wsi);
    void HandleConnectionClose(struct lws* wsi);
    void HandleIncomingMessage(struct lws* wsi, const uint8_t* data, size_t len);
    
    // Room management (internal)
    std::string FindClientRoom(const std::string& client_id) const;
    void BroadcastToRoom(const std::string& room, const std::string& message, bool is_binary = false, const std::string& exclude_client = "");
    void UpdateRoomActivity(const std::string& room);
    
    // Thermal processing
    void CheckThermalThresholds(const std::string& sensor_id, double temperature, double humidity);
    void GenerateThermalAlert(const std::string& sensor_id, const std::string& alert_type, 
                             double value, const std::string& threshold_info);
    
    // Utility methods
    MessageType DetermineMessageType(const std::string& message);
    std::string ExtractSensorId(const std::string& message);
    std::string GenerateClientId();
    bool ValidateMessage(const std::string& message);
    Json::Value CreateAlertMessage(const std::string& sensor_id, const std::string& alert_type, 
                                  const std::string& message);
    
    // Performance monitoring
    void UpdateLatencyMetrics(double latency_ms);
    void UpdateProcessingMetrics(double processing_time_ms);
    void UpdateThroughputMetrics(size_t bytes_count, bool is_sent = true);
    
    // Server management
    bool InitializeLibWebSockets();
    void CleanupLibWebSockets();
    void UpdateServerStatus(ServerStatus status, const std::string& message = "");
    
    // Security and validation
    bool ValidateOrigin(const std::string& origin);
    bool CheckRateLimit(const std::string& client_id);
    bool AuthenticateClient(const std::string& client_id, const std::string& token = "");
    
    // Error handling and logging
    void LogError(const std::string& message);
    void LogInfo(const std::string& message);
    void LogDebug(const std::string& message);
    void LogWarning(const std::string& message);
};

// Utility functions for WebSocket-only operations
namespace WebSocketOnlyUtils {
    // Message utilities
    std::string CreateSensorDataMessage(const std::string& sensor_id, double temperature, 
                                       double humidity, const std::string& location = "");
    std::string CreateAlertMessage(const std::string& sensor_id, const std::string& alert_type,
                                  const std::string& message, double value = 0.0);
    std::string CreateCommandMessage(const std::string& command, const Json::Value& parameters);
    std::string CreateStatusMessage(const std::string& component, const std::string& status,
                                   const Json::Value& details = Json::Value{});
    std::string CreateRoomMessage(const std::string& action, const std::string& room,
                                 const std::string& message = "");
    std::string CreateHeartbeatMessage(const std::string& client_id);
    
    // Parsing utilities
    bool ParseSensorDataMessage(const std::string& message, std::string& sensor_id, 
                               double& temperature, double& humidity, std::string& location);
    bool ParseAlertMessage(const std::string& message, std::string& sensor_id, 
                          std::string& alert_type, std::string& alert_message, double& value);
    bool ParseCommandMessage(const std::string& message, std::string& command, Json::Value& parameters);
    bool ParseRoomMessage(const std::string& message, std::string& action, std::string& room,
                         std::string& content);
    
    // Performance utilities
    double CalculateThroughput(uint64_t message_count, std::chrono::milliseconds duration);
    double CalculateBandwidth(uint64_t bytes_count, std::chrono::milliseconds duration);
    std::string FormatMetrics(const WebSocketOnlyBridge::Metrics& metrics);
    std::string FormatLatencyStats(const std::vector<double>& latencies);
    
    // Configuration utilities
    WebSocketOnlyBridge::Config CreateDefaultConfig();
    WebSocketOnlyBridge::Config CreateHighThroughputConfig();
    WebSocketOnlyBridge::Config CreateLowLatencyConfig();
    WebSocketOnlyBridge::Config CreateSecureConfig(const std::string& cert_path, const std::string& key_path);
    
    // Testing utilities
    bool ValidateServerConnection(const std::string& host, int port, std::chrono::seconds timeout = std::chrono::seconds(5));
    std::vector<std::string> GenerateTestMessages(int message_count);
    void RunPerformanceTest(WebSocketOnlyBridge& bridge, int duration_seconds, int messages_per_second);
    void RunStressTest(WebSocketOnlyBridge& bridge, int concurrent_clients, int duration_seconds);
    void RunLatencyTest(WebSocketOnlyBridge& bridge, int message_count, int interval_ms);
    
    // Validation utilities
    bool ValidateMessageFormat(const std::string& message);
    bool ValidateRoomName(const std::string& room_name);
    bool ValidateClientId(const std::string& client_id);
    
    // Conversion utilities
    std::string ServerStatusToString(WebSocketOnlyBridge::ServerStatus status);
    std::string MessageTypeToString(WebSocketOnlyBridge::MessageType type);
    WebSocketOnlyBridge::MessageType StringToMessageType(const std::string& type_str);
}

#endif // WS_ONLY_BRIDGE_H
