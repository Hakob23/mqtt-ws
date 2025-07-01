#include "ThermalIsolationTracker.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace thermal_monitoring {

//=============================================================================
// ThermalIsolationTracker Implementation
//=============================================================================

ThermalIsolationTracker::ThermalIsolationTracker(const ThermalConfig& config) 
    : config_(config), running_(false) {
    std::cout << "🌡️  ThermalIsolationTracker initialized with " << config_.sensor_locations.size() << " locations" << std::endl;
}

ThermalIsolationTracker::~ThermalIsolationTracker() {
    stop();
}

bool ThermalIsolationTracker::start() {
    if (running_.load()) {
        std::cout << "⚠️  ThermalIsolationTracker already running" << std::endl;
        return false;
    }
    
    running_ = true;
    
    // Start monitoring thread
    monitor_thread_ = std::thread(&ThermalIsolationTracker::monitoring_loop, this);
    
    std::cout << "🚀 ThermalIsolationTracker started" << std::endl;
    return true;
}

void ThermalIsolationTracker::stop() {
    if (running_.load()) {
        running_ = false;
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
        std::cout << "🛑 ThermalIsolationTracker stopped" << std::endl;
    }
}

bool ThermalIsolationTracker::process_sensor_data(const std::string& sensor_id, 
                                                 float temperature, 
                                                 float humidity,
                                                 const std::string& location) {
    std::lock_guard<std::mutex> lock(sensors_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Create or update sensor data
    SensorData& sensor = sensors_[sensor_id];
    
    // Store previous values for rate calculation
    float prev_temp = sensor.temperature;
    auto prev_time = sensor.last_update;
    
    // Update sensor data
    sensor.sensor_id = sensor_id;
    sensor.temperature = temperature;
    sensor.humidity = humidity;
    sensor.location = location.empty() ? get_sensor_location(sensor_id) : location;
    sensor.last_update = now;
    sensor.is_active = true;
    
    // Calculate temperature rate of change
    if (prev_time != std::chrono::steady_clock::time_point{}) {
        auto time_diff = std::chrono::duration_cast<std::chrono::minutes>(now - prev_time);
        if (time_diff.count() > 0) {
            sensor.temp_rate = (temperature - prev_temp) / time_diff.count();
        }
    }
    
    // Add to history
    sensor.temperature_history.push_back({temperature, now});
    if (sensor.temperature_history.size() > config_.history_size) {
        sensor.temperature_history.pop_front();
    }
    
    sensor.humidity_history.push_back({humidity, now});
    if (sensor.humidity_history.size() > config_.history_size) {
        sensor.humidity_history.pop_front();
    }
    
    std::cout << "📊 [" << sensor_id << "] Temp: " << std::fixed << std::setprecision(1) 
              << temperature << "°C, Humidity: " << humidity << "%, Location: " << sensor.location;
    
    if (sensor.temp_rate != 0.0f) {
        std::cout << ", Rate: " << std::setprecision(2) << sensor.temp_rate << "°C/min";
    }
    std::cout << std::endl;
    
    // Check thresholds
    check_thresholds(sensor_id, sensor);
    
    return true;
}

void ThermalIsolationTracker::check_thresholds(const std::string& sensor_id, const SensorData& sensor) {
    std::vector<AlertType> alerts;
    
    // Temperature thresholds
    if (sensor.temperature < config_.temp_min) {
        alerts.push_back(AlertType::TEMP_TOO_LOW);
    } else if (sensor.temperature > config_.temp_max) {
        alerts.push_back(AlertType::TEMP_TOO_HIGH);
    }
    
    // Humidity threshold
    if (sensor.humidity > config_.humidity_max) {
        alerts.push_back(AlertType::HUMIDITY_TOO_HIGH);
    }
    
    // Temperature rate of change
    if (std::abs(sensor.temp_rate) > config_.temp_rate_limit) {
        if (sensor.temp_rate > 0) {
            alerts.push_back(AlertType::TEMP_RISING_FAST);
        } else {
            alerts.push_back(AlertType::TEMP_FALLING_FAST);
        }
    }
    
    // Sensor offline check
    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::minutes>(now - sensor.last_update);
    if (time_since_update.count() > config_.sensor_timeout_minutes) {
        alerts.push_back(AlertType::SENSOR_OFFLINE);
    }
    
    // Generate alerts
    for (AlertType alert : alerts) {
        generate_alert(sensor_id, alert, sensor);
    }
}

void ThermalIsolationTracker::generate_alert(const std::string& sensor_id, 
                                           AlertType alert_type, 
                                           const SensorData& sensor) {
    // Check if we should throttle this alert
    if (should_throttle_alert(sensor_id, alert_type)) {
        return;
    }
    
    Alert alert;
    alert.sensor_id = sensor_id;
    alert.alert_type = alert_type;
    alert.location = sensor.location;
    alert.timestamp = std::chrono::steady_clock::now();
    alert.temperature = sensor.temperature;
    alert.humidity = sensor.humidity;
    alert.temp_rate = sensor.temp_rate;
    
    // Set alert message
    alert.message = format_alert_message(alert);
    
    // Store alert
    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        recent_alerts_.push_back(alert);
        if (recent_alerts_.size() > config_.max_alerts_history) {
            recent_alerts_.pop_front();
        }
    }
    
    // Update alert throttling
    alert_throttle_[sensor_id][alert_type] = alert.timestamp;
    
    // Print alert
    std::cout << "🚨 ALERT [" << sensor_id << "] " << alert.message << std::endl;
    
    // Call alert callback if set
    if (alert_callback_) {
        alert_callback_(alert);
    }
}

std::string ThermalIsolationTracker::format_alert_message(const Alert& alert) {
    std::stringstream ss;
    
    switch (alert.alert_type) {
        case AlertType::TEMP_TOO_LOW:
            ss << "Temperature too low: " << std::fixed << std::setprecision(1) 
               << alert.temperature << "°C (min: " << config_.temp_min << "°C)";
            break;
        case AlertType::TEMP_TOO_HIGH:
            ss << "Temperature too high: " << alert.temperature 
               << "°C (max: " << config_.temp_max << "°C)";
            break;
        case AlertType::HUMIDITY_TOO_HIGH:
            ss << "Humidity too high: " << alert.humidity 
               << "% (max: " << config_.humidity_max << "%)";
            break;
        case AlertType::TEMP_RISING_FAST:
            ss << "Temperature rising rapidly: " << std::setprecision(2) << alert.temp_rate 
               << "°C/min (limit: " << config_.temp_rate_limit << "°C/min)";
            break;
        case AlertType::TEMP_FALLING_FAST:
            ss << "Temperature falling rapidly: " << alert.temp_rate 
               << "°C/min (limit: -" << config_.temp_rate_limit << "°C/min)";
            break;
        case AlertType::SENSOR_OFFLINE:
            ss << "Sensor offline for more than " << config_.sensor_timeout_minutes << " minutes";
            break;
    }
    
    if (!alert.location.empty()) {
        ss << " in " << alert.location;
    }
    
    return ss.str();
}

bool ThermalIsolationTracker::should_throttle_alert(const std::string& sensor_id, AlertType alert_type) {
    auto& sensor_throttle = alert_throttle_[sensor_id];
    auto it = sensor_throttle.find(alert_type);
    
    if (it == sensor_throttle.end()) {
        return false; // First alert of this type
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::minutes>(now - it->second);
    
    return time_since_last.count() < config_.alert_throttle_minutes;
}

std::string ThermalIsolationTracker::get_sensor_location(const std::string& sensor_id) {
    auto it = config_.sensor_locations.find(sensor_id);
    if (it != config_.sensor_locations.end()) {
        return it->second;
    }
    return "Unknown";
}

std::vector<SensorData> ThermalIsolationTracker::get_all_sensors() const {
    std::lock_guard<std::mutex> lock(sensors_mutex_);
    
    std::vector<SensorData> result;
    for (const auto& pair : sensors_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<Alert> ThermalIsolationTracker::get_recent_alerts(int count) const {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    
    std::vector<Alert> result;
    int start = std::max(0, static_cast<int>(recent_alerts_.size()) - count);
    
    for (int i = start; i < static_cast<int>(recent_alerts_.size()); ++i) {
        result.push_back(recent_alerts_[i]);
    }
    
    return result;
}

SensorStats ThermalIsolationTracker::get_sensor_stats(const std::string& sensor_id) const {
    std::lock_guard<std::mutex> lock(sensors_mutex_);
    
    SensorStats stats = {};
    auto it = sensors_.find(sensor_id);
    if (it == sensors_.end()) {
        return stats;
    }
    
    const SensorData& sensor = it->second;
    stats.sensor_id = sensor_id;
    stats.current_temp = sensor.temperature;
    stats.current_humidity = sensor.humidity;
    stats.location = sensor.location;
    
    // Calculate statistics from history
    if (!sensor.temperature_history.empty()) {
        float sum = 0, min_temp = sensor.temperature_history[0].value, max_temp = min_temp;
        
        for (const auto& reading : sensor.temperature_history) {
            sum += reading.value;
            min_temp = std::min(min_temp, reading.value);
            max_temp = std::max(max_temp, reading.value);
        }
        
        stats.avg_temp = sum / sensor.temperature_history.size();
        stats.min_temp = min_temp;
        stats.max_temp = max_temp;
    }
    
    // Calculate uptime
    if (sensor.last_update != std::chrono::steady_clock::time_point{}) {
        auto now = std::chrono::steady_clock::now();
        stats.uptime_minutes = std::chrono::duration_cast<std::chrono::minutes>(
            now - sensor.last_update).count();
    }
    
    return stats;
}

void ThermalIsolationTracker::monitoring_loop() {
    std::cout << "🔄 ThermalIsolationTracker monitoring loop started" << std::endl;
    
    while (running_.load()) {
        // Check for offline sensors
        check_offline_sensors();
        
        // Print status every 30 seconds
        static auto last_status = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status).count() >= 30) {
            print_status();
            last_status = now;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "🏁 ThermalIsolationTracker monitoring loop finished" << std::endl;
}

void ThermalIsolationTracker::check_offline_sensors() {
    std::lock_guard<std::mutex> lock(sensors_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : sensors_) {
        const std::string& sensor_id = pair.first;
        SensorData& sensor = pair.second;
        
        if (sensor.is_active) {
            auto time_since_update = std::chrono::duration_cast<std::chrono::minutes>(
                now - sensor.last_update);
            
            if (time_since_update.count() > config_.sensor_timeout_minutes) {
                sensor.is_active = false;
                generate_alert(sensor_id, AlertType::SENSOR_OFFLINE, sensor);
            }
        }
    }
}

void ThermalIsolationTracker::print_status() {
    std::lock_guard<std::mutex> lock(sensors_mutex_);
    
    int active_sensors = 0;
    float total_temp = 0.0f;
    
    for (const auto& pair : sensors_) {
        if (pair.second.is_active) {
            active_sensors++;
            total_temp += pair.second.temperature;
        }
    }
    
    if (active_sensors > 0) {
        float avg_temp = total_temp / active_sensors;
        std::cout << "📊 Status: " << active_sensors << " active sensors, "
                  << "avg temp: " << std::fixed << std::setprecision(1) << avg_temp << "°C" << std::endl;
    } else {
        std::cout << "📊 Status: No active sensors" << std::endl;
    }
}

//=============================================================================
// Message Parsing Utilities
//=============================================================================

std::optional<SensorReading> parse_sensor_message(const std::string& topic, const std::string& payload) {
    SensorReading reading;
    
    // Parse topic: sensors/{sensor_id}/{data_type}
    std::vector<std::string> topic_parts = split_string(topic, '/');
    if (topic_parts.size() < 3 || topic_parts[0] != "sensors") {
        return std::nullopt;
    }
    
    reading.sensor_id = topic_parts[1];
    std::string data_type = topic_parts[2];
    
    if (data_type == "data") {
        // Parse JSON payload: {"temperature": 25.5, "humidity": 60.2, "location": "room1"}
        return parse_json_sensor_data(payload, reading);
    } else if (data_type == "temperature") {
        reading.temperature = std::stof(payload);
        reading.humidity = 0.0f; // Default
        return reading;
    } else if (data_type == "humidity") {
        reading.humidity = std::stof(payload);
        reading.temperature = 0.0f; // Default
        return reading;
    }
    
    return std::nullopt;
}

std::optional<SensorReading> parse_json_sensor_data(const std::string& json_str, SensorReading& reading) {
    // Simple JSON parser for sensor data
    // Format: {"temperature": 25.5, "humidity": 60.2, "location": "room1"}
    
    try {
        size_t temp_pos = json_str.find("\"temperature\":");
        size_t hum_pos = json_str.find("\"humidity\":");
        size_t loc_pos = json_str.find("\"location\":");
        
        if (temp_pos != std::string::npos) {
            size_t start = json_str.find(':', temp_pos) + 1;
            size_t end = json_str.find_first_of(",}", start);
            reading.temperature = std::stof(json_str.substr(start, end - start));
        }
        
        if (hum_pos != std::string::npos) {
            size_t start = json_str.find(':', hum_pos) + 1;
            size_t end = json_str.find_first_of(",}", start);
            reading.humidity = std::stof(json_str.substr(start, end - start));
        }
        
        if (loc_pos != std::string::npos) {
            size_t start = json_str.find('"', json_str.find(':', loc_pos)) + 1;
            size_t end = json_str.find('"', start);
            reading.location = json_str.substr(start, end - start);
        }
        
        return reading;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error parsing JSON sensor data: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    
    return result;
}

} // namespace thermal_monitoring
