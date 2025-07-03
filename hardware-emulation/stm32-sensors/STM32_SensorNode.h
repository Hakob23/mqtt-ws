#pragma once

#include <string>
#include <chrono>
#include <random>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>

namespace stm32_simulation {

/**
 * Sensor types that can be simulated
 */
enum class SensorType {
    DHT22,          // Temperature + Humidity
    DS18B20,        // Temperature only
    BME280,         // Temperature + Humidity + Pressure
    SHT30,          // Temperature + Humidity (high precision)
    FAULTY_SENSOR,  // Simulates faulty readings
    INTERMITTENT    // Simulates connection issues
};

/**
 * Communication protocols available
 */
enum class CommProtocol {
    UART_TO_GATEWAY,    // Send data via UART to RPi gateway
    MQTT_DIRECT,        // Direct MQTT (if WiFi enabled)
    SPI_TO_GATEWAY,     // SPI communication to gateway
    I2C_TO_GATEWAY      // I2C communication to gateway
};

/**
 * Environmental simulation patterns
 */
enum class EnvironmentPattern {
    INDOOR_STABLE,      // Stable indoor environment
    OUTDOOR_VARIABLE,   // Variable outdoor conditions
    HEATING_CYCLE,      // HVAC heating cycle
    COOLING_CYCLE,      // HVAC cooling cycle
    INDUSTRIAL,         // Industrial environment
    GREENHOUSE,         // Greenhouse conditions
    SERVER_ROOM,        // Data center/server room
    BASEMENT,           // Basement conditions
    ATTIC              // Attic conditions
};

/**
 * Sensor reading structure (raw data from sensor)
 */
struct SensorReading {
    float temperature_celsius;
    float humidity_percent;
    float pressure_hpa;         // Only for BME280
    bool is_valid;
    std::chrono::steady_clock::time_point timestamp;
    
    // Sensor-specific metadata
    uint16_t raw_temp_adc;      // Raw ADC reading
    uint16_t raw_humidity_adc;
    uint8_t sensor_status;      // Status register
    float supply_voltage;       // Power supply voltage
};

/**
 * Configuration for a sensor node
 */
struct SensorNodeConfig {
    std::string node_id;
    std::string location;
    SensorType sensor_type;
    CommProtocol comm_protocol;
    EnvironmentPattern environment;
    
    // Timing configuration
    int reading_interval_ms = 2000;    // How often to read sensor
    int transmission_interval_ms = 5000; // How often to transmit
    
    // Environmental parameters
    float base_temperature = 22.0f;
    float base_humidity = 50.0f;
    float temp_variation = 2.0f;
    float humidity_variation = 5.0f;
    
    // Hardware simulation
    float sensor_accuracy = 0.1f;      // ±0.1°C accuracy
    float noise_level = 0.05f;         // Sensor noise
    float drift_rate = 0.001f;         // Long-term drift
    float power_variation = 0.1f;      // Power supply variation
    
    // Fault simulation
    float fault_probability = 0.001f;  // Probability of fault per reading
    float connection_stability = 0.99f; // Connection reliability
    
    // Communication settings
    std::string gateway_address = "192.168.1.100";
    int gateway_port = 8888;
    std::string mqtt_broker = "localhost";
    int mqtt_port = 1883;
};

/**
 * STM32 Sensor Node Simulator
 * Simulates a complete STM32 microcontroller with environmental sensors
 */
class STM32_SensorNode {
public:
    explicit STM32_SensorNode(const SensorNodeConfig& config);
    ~STM32_SensorNode();
    
    // Main interface
    bool initialize();
    bool start();
    void stop();
    
    // Status queries
    bool is_running() const { return running_.load(); }
    std::string get_node_id() const { return config_.node_id; }
    std::string get_status() const;
    
    // Data access
    SensorReading get_last_reading() const;
    std::vector<SensorReading> get_reading_history(int count = 10) const;
    
    // Callbacks for data transmission
    void set_uart_callback(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback);
    void set_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback);
    
    // Simulation controls
    void inject_fault();
    void simulate_power_loss(int duration_ms);
    void change_environment(EnvironmentPattern new_pattern);
    void update_base_conditions(float temp, float humidity);
    
private:
    SensorNodeConfig config_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Threading
    std::thread sensor_thread_;
    std::thread transmission_thread_;
    mutable std::mutex data_mutex_;
    
    // Sensor simulation
    std::mt19937 random_generator_;
    std::normal_distribution<float> temp_noise_;
    std::normal_distribution<float> humidity_noise_;
    std::uniform_real_distribution<float> fault_dist_;
    
    // Environmental simulation
    float current_base_temp_;
    float current_base_humidity_;
    float temp_trend_;
    float humidity_trend_;
    std::chrono::steady_clock::time_point start_time_;
    
    // Data storage
    SensorReading last_reading_;
    std::vector<SensorReading> reading_history_;
    static const size_t MAX_HISTORY = 100;
    
    // Sensor state
    bool sensor_fault_;
    bool connection_fault_;
    float sensor_drift_;
    float supply_voltage_;
    uint32_t reading_count_;
    uint32_t transmission_count_;
    
    // Communication callbacks
    std::function<void(const std::string&, const std::vector<uint8_t>&)> uart_callback_;
    std::function<void(const std::string&, const std::string&)> mqtt_callback_;
    
    // Internal methods
    void sensor_reading_loop();
    void transmission_loop();
    
    // Sensor simulation
    SensorReading read_sensor();
    float simulate_temperature();
    float simulate_humidity();
    float apply_environmental_pattern(float base_value, float time_hours, bool is_temperature);
    bool check_sensor_fault();
    bool check_connection_fault();
    
    // Data formatting
    std::string format_uart_message(const SensorReading& reading);
    std::string format_mqtt_message(const SensorReading& reading);
    std::vector<uint8_t> create_binary_packet(const SensorReading& reading);
    
    // Hardware simulation
    uint16_t temperature_to_adc(float temp_celsius);
    uint16_t humidity_to_adc(float humidity_percent);
    float simulate_supply_voltage();
    uint8_t get_sensor_status();
    
    // Utility methods
    std::string get_sensor_type_string() const;
    std::string get_comm_protocol_string() const;
    std::string get_environment_pattern_string() const;
};

/**
 * Multi-sensor deployment manager
 * Manages multiple sensor nodes in a coordinated deployment
 */
class SensorDeployment {
public:
    SensorDeployment() = default;
    ~SensorDeployment();
    
    // Deployment management
    void add_sensor_node(std::unique_ptr<STM32_SensorNode> node);
    void remove_sensor_node(const std::string& node_id);
    
    bool start_all();
    void stop_all();
    
    // Status and monitoring
    size_t get_node_count() const { return sensor_nodes_.size(); }
    std::vector<std::string> get_node_ids() const;
    std::string get_deployment_status() const;
    
    // Bulk operations
    void simulate_power_outage(int duration_ms);
    void change_all_environments(EnvironmentPattern pattern);
    void inject_random_faults(float fault_rate = 0.1f);
    
    // Data collection
    std::vector<SensorReading> collect_all_readings() const;
    void save_deployment_log(const std::string& filename) const;
    
    // Communication setup
    void set_global_uart_callback(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback);
    void set_global_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback);
    
private:
    std::vector<std::unique_ptr<STM32_SensorNode>> sensor_nodes_;
    mutable std::mutex nodes_mutex_;
    
    // Global callbacks
    std::function<void(const std::string&, const std::vector<uint8_t>&)> global_uart_callback_;
    std::function<void(const std::string&, const std::string&)> global_mqtt_callback_;
};

/**
 * Factory functions for common sensor configurations
 */
namespace sensor_factory {
    SensorNodeConfig create_indoor_node(const std::string& id, const std::string& location);
    SensorNodeConfig create_outdoor_node(const std::string& id, const std::string& location);
    SensorNodeConfig create_industrial_node(const std::string& id, const std::string& location);
    SensorNodeConfig create_greenhouse_node(const std::string& id, const std::string& location);
    SensorNodeConfig create_server_room_node(const std::string& id, const std::string& location);
    
    // Create a typical deployment
    std::unique_ptr<SensorDeployment> create_home_deployment();
    std::unique_ptr<SensorDeployment> create_office_deployment();
    std::unique_ptr<SensorDeployment> create_industrial_deployment();
    std::unique_ptr<SensorDeployment> create_agricultural_deployment();
}

} // namespace stm32_simulation 