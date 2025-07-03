#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <optional>

namespace thermal_monitoring {

/**
 * Alert types for temperature monitoring
 */
enum class AlertType {
    TEMP_TOO_LOW,
    TEMP_TOO_HIGH,
    HUMIDITY_TOO_HIGH,
    TEMP_RISING_FAST,
    TEMP_FALLING_FAST,
    SENSOR_OFFLINE
};

/**
 * Configuration for thermal monitoring
 */
struct ThermalConfig {
    // Temperature thresholds (Celsius)
    float temp_min = 18.0f;
    float temp_max = 28.0f;
    float humidity_max = 60.0f;
    
    // Rate of change limit (degrees per minute)
    float temp_rate_limit = 2.0f;
    
    // Timing settings
    int sensor_timeout_minutes = 10;
    int alert_throttle_minutes = 5;
    
    // History settings
    size_t history_size = 100;
    size_t max_alerts_history = 1000;
    
    // Sensor locations mapping
    std::unordered_map<std::string, std::string> sensor_locations;
};

/**
 * Time-stamped sensor reading
 */
struct TimestampedReading {
    float value;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * Sensor data structure
 */
struct SensorData {
    std::string sensor_id;
    float temperature = 0.0f;
    float humidity = 0.0f;
    std::string location;
    std::chrono::steady_clock::time_point last_update;
    bool is_active = false;
    float temp_rate = 0.0f; // degrees per minute
    
    // Historical data
    std::deque<TimestampedReading> temperature_history;
    std::deque<TimestampedReading> humidity_history;
};

/**
 * Alert structure
 */
struct Alert {
    std::string sensor_id;
    AlertType alert_type;
    std::string location;
    std::chrono::steady_clock::time_point timestamp;
    float temperature;
    float humidity;
    float temp_rate;
    std::string message;
};

/**
 * Sensor statistics
 */
struct SensorStats {
    std::string sensor_id;
    std::string location;
    float current_temp = 0.0f;
    float current_humidity = 0.0f;
    float avg_temp = 0.0f;
    float min_temp = 0.0f;
    float max_temp = 0.0f;
    int uptime_minutes = 0;
};

/**
 * Parsed sensor reading from MQTT message
 */
struct SensorReading {
    std::string sensor_id;
    float temperature = 0.0f;
    float humidity = 0.0f;
    std::string location;
};

/**
 * Main thermal isolation tracking class
 */
class ThermalIsolationTracker {
public:
    explicit ThermalIsolationTracker(const ThermalConfig& config);
    ~ThermalIsolationTracker();
    
    // Main interface
    bool start();
    void stop();
    
    // Sensor data processing
    bool process_sensor_data(const std::string& sensor_id, 
                           float temperature, 
                           float humidity,
                           const std::string& location = "");
    
    // Data retrieval
    std::vector<SensorData> get_all_sensors() const;
    std::vector<Alert> get_recent_alerts(int count = 10) const;
    SensorStats get_sensor_stats(const std::string& sensor_id) const;
    
    // Alert callback
    void set_alert_callback(std::function<void(const Alert&)> callback) {
        alert_callback_ = callback;
    }
    
private:
    ThermalConfig config_;
    std::atomic<bool> running_;
    std::thread monitor_thread_;
    
    // Sensor data storage
    mutable std::mutex sensors_mutex_;
    std::unordered_map<std::string, SensorData> sensors_;
    
    // Alert storage
    mutable std::mutex alerts_mutex_;
    std::deque<Alert> recent_alerts_;
    
    // Alert throttling
    std::unordered_map<std::string, 
        std::unordered_map<AlertType, std::chrono::steady_clock::time_point>> alert_throttle_;
    
    // Alert callback
    std::function<void(const Alert&)> alert_callback_;
    
    // Internal methods
    void monitoring_loop();
    void check_thresholds(const std::string& sensor_id, const SensorData& sensor);
    void generate_alert(const std::string& sensor_id, AlertType alert_type, const SensorData& sensor);
    void check_offline_sensors();
    void print_status();
    
    // Utility methods
    std::string format_alert_message(const Alert& alert);
    bool should_throttle_alert(const std::string& sensor_id, AlertType alert_type);
    std::string get_sensor_location(const std::string& sensor_id);
};

/**
 * Message parsing utilities
 */
std::optional<SensorReading> parse_sensor_message(const std::string& topic, const std::string& payload);
std::optional<SensorReading> parse_json_sensor_data(const std::string& json_str, SensorReading& reading);
std::vector<std::string> split_string(const std::string& str, char delimiter);

} // namespace thermal_monitoring 