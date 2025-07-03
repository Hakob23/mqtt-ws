#pragma once

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>
#include <fstream>
#include <unordered_map>

namespace rpi4_gateway {

/**
 * Communication interface types for STM32 sensor nodes
 */
enum class CommInterface {
    UART_INTERFACE,
    SPI_INTERFACE,
    I2C_INTERFACE,
    USB_SERIAL,
    GPIO_CUSTOM
};

/**
 * Gateway operation modes
 */
enum class GatewayMode {
    COLLECTOR_ONLY,     // Just collect and forward data
    EDGE_PROCESSOR,     // Local processing and analytics
    HYBRID_BRIDGE,      // Full bridge with thermal monitoring
    FAILSAFE_MODE       // Minimal operation during issues
};

/**
 * Data processing strategies
 */
enum class ProcessingStrategy {
    RAW_FORWARD,        // Forward all data as-is
    AGGREGATE_BATCH,    // Batch and aggregate data
    SMART_FILTER,       // Intelligent filtering and prioritization
    PREDICTIVE_EDGE     // Edge AI/ML processing
};

/**
 * Sensor data packet from STM32 nodes
 */
struct SensorDataPacket {
    std::string sensor_id;
    std::string location;
    float temperature_celsius;
    float humidity_percent;
    float pressure_hpa;         // Optional (BME280)
    float supply_voltage;
    uint8_t sensor_status;
    uint16_t raw_temp_adc;
    uint16_t raw_humidity_adc;
    std::chrono::steady_clock::time_point timestamp;
    CommInterface interface_used;
    bool is_valid;
    
    // Quality metrics
    float signal_strength;      // For wireless interfaces
    uint32_t packet_sequence;   // For reliability tracking
    float data_confidence;      // AI-computed confidence score
};

/**
 * Aggregated sensor statistics
 */
struct SensorStatistics {
    std::string sensor_id;
    size_t total_packets;
    size_t valid_packets;
    size_t error_packets;
    float packet_loss_rate;
    float avg_temperature;
    float avg_humidity;
    float min_temperature;
    float max_temperature;
    float temperature_stddev;
    std::chrono::steady_clock::time_point last_update;
    std::chrono::steady_clock::time_point first_seen;
};

/**
 * Gateway system status
 */
struct GatewayStatus {
    bool is_running;
    GatewayMode current_mode;
    ProcessingStrategy processing_strategy;
    
    // Interface status
    bool uart_active;
    bool spi_active;
    bool i2c_active;
    bool mqtt_connected;
    bool websocket_active;
    
    // Performance metrics
    uint32_t total_sensors_active;
    uint32_t packets_processed_per_second;
    uint32_t mqtt_messages_sent;
    uint32_t websocket_messages_sent;
    float cpu_usage_percent;
    uint64_t memory_usage_bytes;
    float disk_usage_percent;
    
    // Network status
    std::string network_interface;
    std::string ip_address;
    bool internet_connectivity;
    
    std::chrono::steady_clock::time_point boot_time;
    std::chrono::steady_clock::time_point last_status_update;
};

/**
 * Edge processing result
 */
struct EdgeProcessingResult {
    std::string sensor_id;
    std::string analysis_type;
    std::map<std::string, float> metrics;
    std::vector<std::string> alerts;
    std::vector<std::string> recommendations;
    float confidence_score;
    std::chrono::steady_clock::time_point processed_at;
};

/**
 * Configuration for the RPi4 Gateway
 */
struct RPi4GatewayConfig {
    // Basic settings
    std::string gateway_id;
    std::string location;
    GatewayMode mode;
    ProcessingStrategy processing_strategy;
    
    // Communication interfaces
    std::string uart_device = "/dev/ttyAMA0";
    int uart_baudrate = 115200;
    std::string spi_device = "/dev/spidev0.0";
    int spi_speed = 1000000;
    int i2c_bus = 1;
    std::vector<int> i2c_addresses;
    
    // MQTT settings
    std::string mqtt_broker = "localhost";
    int mqtt_port = 1883;
    std::string mqtt_username;
    std::string mqtt_password;
    std::string mqtt_base_topic = "gateway";
    bool mqtt_ssl = false;
    
    // WebSocket settings
    std::string websocket_host = "localhost";
    int websocket_port = 8080;
    std::string websocket_path = "/ws";
    bool websocket_ssl = false;
    
    // Processing settings
    int processing_interval_ms = 1000;
    int aggregation_window_seconds = 60;
    int max_sensor_history = 1000;
    bool enable_edge_analytics = true;
    bool enable_local_storage = true;
    
    // Storage settings
    std::string data_directory = "/var/lib/rpi4-gateway";
    std::string log_directory = "/var/log/rpi4-gateway";
    int max_log_files = 10;
    uint64_t max_storage_mb = 1024;
    
    // Alert thresholds
    float temp_alert_low = 10.0f;
    float temp_alert_high = 35.0f;
    float humidity_alert_high = 80.0f;
    float packet_loss_alert_threshold = 0.1f;
    
    // System limits
    int max_concurrent_sensors = 100;
    int max_queue_size = 10000;
    int worker_thread_count = 4;
};

/**
 * Communication interface base class
 */
class CommInterfaceBase {
public:
    virtual ~CommInterfaceBase() = default;
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool is_active() const = 0;
    virtual std::string get_interface_name() const = 0;
    virtual void set_data_callback(std::function<void(const SensorDataPacket&)> callback) = 0;
};

/**
 * UART Communication Interface
 */
class UARTInterface : public CommInterfaceBase {
public:
    explicit UARTInterface(const std::string& device, int baudrate);
    ~UARTInterface();
    
    bool initialize() override;
    bool start() override;
    void stop() override;
    bool is_active() const override { return active_.load(); }
    std::string get_interface_name() const override { return "UART"; }
    void set_data_callback(std::function<void(const SensorDataPacket&)> callback) override;
    
private:
    std::string device_;
    int baudrate_;
    int fd_;
    std::atomic<bool> active_;
    std::thread reader_thread_;
    std::function<void(const SensorDataPacket&)> data_callback_;
    
    void reader_loop();
    SensorDataPacket parse_uart_packet(const std::vector<uint8_t>& data);
};

/**
 * SPI Communication Interface  
 */
class SPIInterface : public CommInterfaceBase {
public:
    explicit SPIInterface(const std::string& device, int speed);
    ~SPIInterface();
    
    bool initialize() override;
    bool start() override;
    void stop() override;
    bool is_active() const override { return active_.load(); }
    std::string get_interface_name() const override { return "SPI"; }
    void set_data_callback(std::function<void(const SensorDataPacket&)> callback) override;
    
private:
    std::string device_;
    int speed_;
    int fd_;
    std::atomic<bool> active_;
    std::thread polling_thread_;
    std::function<void(const SensorDataPacket&)> data_callback_;
    
    void polling_loop();
    SensorDataPacket parse_spi_packet(const std::vector<uint8_t>& data);
};

/**
 * I2C Communication Interface
 */
class I2CInterface : public CommInterfaceBase {
public:
    explicit I2CInterface(int bus, const std::vector<int>& addresses);
    ~I2CInterface();
    
    bool initialize() override;
    bool start() override;
    void stop() override;
    bool is_active() const override { return active_.load(); }
    std::string get_interface_name() const override { return "I2C"; }
    void set_data_callback(std::function<void(const SensorDataPacket&)> callback) override;
    
private:
    int bus_;
    std::vector<int> addresses_;
    int fd_;
    std::atomic<bool> active_;
    std::thread polling_thread_;
    std::function<void(const SensorDataPacket&)> data_callback_;
    
    void polling_loop();
    bool read_i2c_sensor(int address, std::vector<uint8_t>& data);
    SensorDataPacket parse_i2c_packet(int address, const std::vector<uint8_t>& data);
};

/**
 * Data processing engine
 */
class DataProcessor {
public:
    explicit DataProcessor(const RPi4GatewayConfig& config);
    ~DataProcessor();
    
    bool initialize();
    bool start();
    void stop();
    
    // Data input
    void process_packet(const SensorDataPacket& packet);
    
    // Statistics and monitoring
    SensorStatistics get_sensor_statistics(const std::string& sensor_id) const;
    std::vector<SensorStatistics> get_all_statistics() const;
    
    // Edge processing
    void enable_edge_analytics(bool enable) { edge_analytics_enabled_ = enable; }
    std::vector<EdgeProcessingResult> get_recent_edge_results(int count = 10) const;
    
    // Callbacks
    void set_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback);
    void set_websocket_callback(std::function<void(const std::string&)> callback);
    void set_alert_callback(std::function<void(const std::string&, const std::string&)> callback);
    
private:
    RPi4GatewayConfig config_;
    std::atomic<bool> running_;
    
    // Threading
    std::vector<std::thread> worker_threads_;
    std::queue<SensorDataPacket> processing_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Data storage
    std::unordered_map<std::string, std::vector<SensorDataPacket>> sensor_history_;
    std::unordered_map<std::string, SensorStatistics> sensor_stats_;
    mutable std::mutex data_mutex_;
    
    // Edge processing
    std::atomic<bool> edge_analytics_enabled_;
    std::vector<EdgeProcessingResult> edge_results_;
    mutable std::mutex edge_results_mutex_;
    
    // Callbacks
    std::function<void(const std::string&, const std::string&)> mqtt_callback_;
    std::function<void(const std::string&)> websocket_callback_;
    std::function<void(const std::string&, const std::string&)> alert_callback_;
    
    // Internal methods
    void worker_loop();
    void process_packet_internal(const SensorDataPacket& packet);
    void update_statistics(const SensorDataPacket& packet);
    void check_alerts(const SensorDataPacket& packet);
    void perform_edge_analytics(const SensorDataPacket& packet);
    void aggregate_and_forward();
    
    // Data formatting
    std::string format_mqtt_message(const SensorDataPacket& packet);
    std::string format_websocket_message(const SensorDataPacket& packet);
    std::string format_aggregated_data(const std::vector<SensorDataPacket>& packets);
};

/**
 * Local storage manager
 */
class StorageManager {
public:
    explicit StorageManager(const RPi4GatewayConfig& config);
    ~StorageManager();
    
    bool initialize();
    void cleanup();
    
    // Data storage
    bool store_sensor_data(const SensorDataPacket& packet);
    bool store_statistics(const SensorStatistics& stats);
    bool store_edge_result(const EdgeProcessingResult& result);
    
    // Data retrieval
    std::vector<SensorDataPacket> retrieve_sensor_data(const std::string& sensor_id, 
                                                       const std::chrono::system_clock::time_point& start,
                                                       const std::chrono::system_clock::time_point& end);
    
    // Maintenance
    void rotate_logs();
    void cleanup_old_data();
    uint64_t get_storage_usage() const;
    
private:
    RPi4GatewayConfig config_;
    std::string data_path_;
    std::string log_path_;
    mutable std::mutex storage_mutex_;
    
    bool ensure_directories();
    std::string get_data_filename(const std::string& sensor_id, const std::chrono::system_clock::time_point& timestamp);
};

/**
 * System monitor for RPi4 performance
 */
class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();
    
    bool start();
    void stop();
    
    GatewayStatus get_system_status() const;
    float get_cpu_usage() const;
    uint64_t get_memory_usage() const;
    float get_disk_usage() const;
    std::string get_network_info() const;
    bool check_internet_connectivity() const;
    
private:
    std::atomic<bool> running_;
    std::thread monitor_thread_;
    mutable std::mutex status_mutex_;
    GatewayStatus current_status_;
    
    void monitor_loop();
    void update_system_metrics();
};

/**
 * Main RPi4 Gateway class
 */
class RPi4_Gateway {
public:
    explicit RPi4_Gateway(const RPi4GatewayConfig& config);
    ~RPi4_Gateway();
    
    // Main interface
    bool initialize();
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Status and monitoring
    GatewayStatus get_status() const;
    std::vector<SensorStatistics> get_sensor_statistics() const;
    std::vector<EdgeProcessingResult> get_edge_results() const;
    
    // Configuration
    void update_config(const RPi4GatewayConfig& new_config);
    RPi4GatewayConfig get_config() const { return config_; }
    
    // Manual operations
    void trigger_data_sync();
    void perform_system_cleanup();
    void switch_mode(GatewayMode new_mode);
    
    // Callbacks for external integration
    void set_external_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback);
    void set_external_websocket_callback(std::function<void(const std::string&)> callback);
    void set_thermal_monitoring_callback(std::function<void(const std::string&, float, float)> callback);
    
private:
    RPi4GatewayConfig config_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Core components
    std::unique_ptr<DataProcessor> data_processor_;
    std::unique_ptr<StorageManager> storage_manager_;
    std::unique_ptr<SystemMonitor> system_monitor_;
    
    // Communication interfaces
    std::vector<std::unique_ptr<CommInterfaceBase>> comm_interfaces_;
    
    // Threading
    std::thread main_loop_thread_;
    mutable std::mutex gateway_mutex_;
    
    // Callbacks
    std::function<void(const std::string&, const std::string&)> external_mqtt_callback_;
    std::function<void(const std::string&)> external_websocket_callback_;
    std::function<void(const std::string&, float, float)> thermal_callback_;
    
    // Internal methods
    void main_loop();
    void setup_communication_interfaces();
    void handle_sensor_data(const SensorDataPacket& packet);
    void handle_mqtt_message(const std::string& topic, const std::string& message);
    void handle_websocket_message(const std::string& message);
    void handle_alert(const std::string& alert_type, const std::string& message);
    
    // Integration with existing thermal monitoring
    void integrate_with_thermal_system(const SensorDataPacket& packet);
};

/**
 * Factory functions for common gateway configurations
 */
namespace gateway_factory {
    RPi4GatewayConfig create_home_gateway_config(const std::string& gateway_id);
    RPi4GatewayConfig create_industrial_gateway_config(const std::string& gateway_id);
    RPi4GatewayConfig create_agricultural_gateway_config(const std::string& gateway_id);
    RPi4GatewayConfig create_edge_ai_gateway_config(const std::string& gateway_id);
    
    std::unique_ptr<RPi4_Gateway> create_basic_gateway(const std::string& gateway_id);
    std::unique_ptr<RPi4_Gateway> create_full_featured_gateway(const std::string& gateway_id);
}

} // namespace rpi4_gateway 