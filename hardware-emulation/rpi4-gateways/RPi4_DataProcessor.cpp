#include "RPi4_Gateway.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>

namespace rpi4_gateway {

//=============================================================================
// DataProcessor Implementation
//=============================================================================

DataProcessor::DataProcessor(const RPi4GatewayConfig& config)
    : config_(config), running_(false), edge_analytics_enabled_(config.enable_edge_analytics) {
    std::cout << "🧠 [DataProcessor] Created with " << config_.worker_thread_count 
              << " worker threads" << std::endl;
}

DataProcessor::~DataProcessor() {
    stop();
    std::cout << "🧠 [DataProcessor] Destroyed" << std::endl;
}

bool DataProcessor::initialize() {
    std::cout << "🚀 [DataProcessor] Initializing..." << std::endl;
    
    // Initialize data structures
    sensor_history_.clear();
    sensor_stats_.clear();
    edge_results_.clear();
    
    std::cout << "✅ [DataProcessor] Initialized successfully" << std::endl;
    return true;
}

bool DataProcessor::start() {
    if (running_.load()) {
        std::cout << "⚠️ [DataProcessor] Already running" << std::endl;
        return true;
    }
    
    running_ = true;
    
    // Start worker threads
    worker_threads_.clear();
    for (int i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.emplace_back(&DataProcessor::worker_loop, this);
    }
    
    std::cout << "🚀 [DataProcessor] Started with " << config_.worker_thread_count 
              << " worker threads" << std::endl;
    return true;
}

void DataProcessor::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "🛑 [DataProcessor] Stopping..." << std::endl;
    running_ = false;
    
    // Wake up all worker threads
    queue_cv_.notify_all();
    
    // Wait for all threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    std::cout << "✅ [DataProcessor] Stopped" << std::endl;
}

void DataProcessor::process_packet(const SensorDataPacket& packet) {
    if (!running_.load()) {
        return;
    }
    
    // Add packet to processing queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (processing_queue_.size() >= static_cast<size_t>(config_.max_queue_size)) {
            std::cout << "⚠️ [DataProcessor] Queue full, dropping packet from " 
                      << packet.sensor_id << std::endl;
            return;
        }
        processing_queue_.push(packet);
    }
    
    // Notify worker threads
    queue_cv_.notify_one();
}

SensorStatistics DataProcessor::get_sensor_statistics(const std::string& sensor_id) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = sensor_stats_.find(sensor_id);
    if (it != sensor_stats_.end()) {
        return it->second;
    }
    
    // Return empty statistics if sensor not found
    SensorStatistics empty_stats = {};
    empty_stats.sensor_id = sensor_id;
    return empty_stats;
}

std::vector<SensorStatistics> DataProcessor::get_all_statistics() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<SensorStatistics> stats;
    for (const auto& pair : sensor_stats_) {
        stats.push_back(pair.second);
    }
    
    return stats;
}

std::vector<EdgeProcessingResult> DataProcessor::get_recent_edge_results(int count) const {
    std::lock_guard<std::mutex> lock(edge_results_mutex_);
    
    std::vector<EdgeProcessingResult> results;
    int start_index = std::max(0, static_cast<int>(edge_results_.size()) - count);
    
    for (int i = start_index; i < static_cast<int>(edge_results_.size()); ++i) {
        results.push_back(edge_results_[i]);
    }
    
    return results;
}

void DataProcessor::set_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback) {
    mqtt_callback_ = callback;
}

void DataProcessor::set_websocket_callback(std::function<void(const std::string&)> callback) {
    websocket_callback_ = callback;
}

void DataProcessor::set_alert_callback(std::function<void(const std::string&, const std::string&)> callback) {
    alert_callback_ = callback;
}

void DataProcessor::worker_loop() {
    std::cout << "🏃 [DataProcessor] Worker thread started" << std::endl;
    
    while (running_.load()) {
        SensorDataPacket packet;
        bool has_packet = false;
        
        // Get packet from queue
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !processing_queue_.empty() || !running_.load(); });
            
            if (!running_.load()) {
                break;
            }
            
            if (!processing_queue_.empty()) {
                packet = processing_queue_.front();
                processing_queue_.pop();
                has_packet = true;
            }
        }
        
        if (has_packet) {
            process_packet_internal(packet);
        }
    }
    
    std::cout << "🏁 [DataProcessor] Worker thread finished" << std::endl;
}

void DataProcessor::process_packet_internal(const SensorDataPacket& packet) {
    if (!packet.is_valid) {
        std::cout << "⚠️ [DataProcessor] Ignoring invalid packet from " << packet.sensor_id << std::endl;
        return;
    }
    
    // Update statistics
    update_statistics(packet);
    
    // Store in history
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto& history = sensor_history_[packet.sensor_id];
        history.push_back(packet);
        
        // Limit history size
        if (history.size() > static_cast<size_t>(config_.max_sensor_history)) {
            history.erase(history.begin());
        }
    }
    
    // Check for alerts
    check_alerts(packet);
    
    // Perform edge analytics
    if (edge_analytics_enabled_.load()) {
        perform_edge_analytics(packet);
    }
    
    // Forward to MQTT
    if (mqtt_callback_) {
        std::string topic = config_.mqtt_base_topic + "/sensors/" + packet.sensor_id + "/data";
        std::string message = format_mqtt_message(packet);
        mqtt_callback_(topic, message);
    }
    
    // Forward to WebSocket
    if (websocket_callback_) {
        std::string message = format_websocket_message(packet);
        websocket_callback_(message);
    }
    
    // Periodic aggregation
    static auto last_aggregation = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (now - last_aggregation >= std::chrono::seconds(config_.aggregation_window_seconds)) {
        aggregate_and_forward();
        last_aggregation = now;
    }
}

void DataProcessor::update_statistics(const SensorDataPacket& packet) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto& stats = sensor_stats_[packet.sensor_id];
    
    // Initialize if first packet
    if (stats.sensor_id.empty()) {
        stats.sensor_id = packet.sensor_id;
        stats.total_packets = 0;
        stats.valid_packets = 0;
        stats.error_packets = 0;
        stats.packet_loss_rate = 0.0f;
        stats.min_temperature = packet.temperature_celsius;
        stats.max_temperature = packet.temperature_celsius;
        stats.first_seen = packet.timestamp;
    }
    
    // Update counters
    stats.total_packets++;
    if (packet.is_valid) {
        stats.valid_packets++;
    } else {
        stats.error_packets++;
    }
    
    // Update packet loss rate
    stats.packet_loss_rate = static_cast<float>(stats.error_packets) / stats.total_packets;
    
    // Update temperature statistics
    if (packet.is_valid) {
        stats.min_temperature = std::min(stats.min_temperature, packet.temperature_celsius);
        stats.max_temperature = std::max(stats.max_temperature, packet.temperature_celsius);
        
        // Calculate running average (simplified)
        stats.avg_temperature = (stats.avg_temperature * (stats.valid_packets - 1) + 
                                packet.temperature_celsius) / stats.valid_packets;
        stats.avg_humidity = (stats.avg_humidity * (stats.valid_packets - 1) + 
                             packet.humidity_percent) / stats.valid_packets;
        
        // Calculate standard deviation (simplified)
        // In a real implementation, you'd use a more sophisticated approach
        stats.temperature_stddev = std::abs(packet.temperature_celsius - stats.avg_temperature);
    }
    
    stats.last_update = packet.timestamp;
}

void DataProcessor::check_alerts(const SensorDataPacket& packet) {
    std::vector<std::string> alerts;
    
    // Temperature alerts
    if (packet.temperature_celsius < config_.temp_alert_low) {
        alerts.push_back("Temperature too low: " + std::to_string(packet.temperature_celsius) + "°C");
    }
    if (packet.temperature_celsius > config_.temp_alert_high) {
        alerts.push_back("Temperature too high: " + std::to_string(packet.temperature_celsius) + "°C");
    }
    
    // Humidity alerts
    if (packet.humidity_percent > config_.humidity_alert_high) {
        alerts.push_back("Humidity too high: " + std::to_string(packet.humidity_percent) + "%");
    }
    
    // Voltage alerts
    if (packet.supply_voltage < 3.0f) {
        alerts.push_back("Low battery voltage: " + std::to_string(packet.supply_voltage) + "V");
    }
    
    // Sensor status alerts
    if (packet.sensor_status & 0x01) {
        alerts.push_back("Sensor fault detected");
    }
    if (packet.sensor_status & 0x02) {
        alerts.push_back("Communication fault");
    }
    
    // Send alerts
    for (const auto& alert : alerts) {
        if (alert_callback_) {
            alert_callback_("SENSOR_ALERT", packet.sensor_id + ": " + alert);
        }
        std::cout << "🚨 [DataProcessor] ALERT - " << packet.sensor_id << ": " << alert << std::endl;
    }
}

void DataProcessor::perform_edge_analytics(const SensorDataPacket& packet) {
    // Get sensor history for analysis
    std::vector<SensorDataPacket> history;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = sensor_history_.find(packet.sensor_id);
        if (it != sensor_history_.end()) {
            history = it->second;
        }
    }
    
    if (history.size() < 5) {
        return; // Need at least 5 data points for meaningful analysis
    }
    
    EdgeProcessingResult result;
    result.sensor_id = packet.sensor_id;
    result.analysis_type = "trend_analysis";
    result.processed_at = std::chrono::steady_clock::now();
    
    // Calculate temperature trend
    std::vector<float> temperatures;
    for (const auto& h : history) {
        temperatures.push_back(h.temperature_celsius);
    }
    
    // Simple linear regression for trend
    float n = static_cast<float>(temperatures.size());
    float sum_x = n * (n - 1) / 2.0f;
    float sum_y = std::accumulate(temperatures.begin(), temperatures.end(), 0.0f);
    float sum_xy = 0.0f;
    float sum_x2 = 0.0f;
    
    for (size_t i = 0; i < temperatures.size(); ++i) {
        float x = static_cast<float>(i);
        float y = temperatures[i];
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    float slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    float intercept = (sum_y - slope * sum_x) / n;
    
    result.metrics["temperature_trend_slope"] = slope;
    result.metrics["temperature_trend_intercept"] = intercept;
    result.metrics["temperature_current"] = packet.temperature_celsius;
    result.metrics["humidity_current"] = packet.humidity_percent;
    
    // Calculate confidence based on data quality
    result.confidence_score = std::min(1.0f, packet.data_confidence * (history.size() / 10.0f));
    
    // Generate alerts and recommendations
    if (std::abs(slope) > 0.5f) {
        if (slope > 0) {
            result.alerts.push_back("Rising temperature trend detected");
            result.recommendations.push_back("Monitor for overheating");
        } else {
            result.alerts.push_back("Falling temperature trend detected");
            result.recommendations.push_back("Check heating system");
        }
    }
    
    if (packet.humidity_percent > 70.0f) {
        result.alerts.push_back("High humidity detected");
        result.recommendations.push_back("Improve ventilation");
    }
    
    // Store result
    {
        std::lock_guard<std::mutex> lock(edge_results_mutex_);
        edge_results_.push_back(result);
        
        // Limit results history
        if (edge_results_.size() > 100) {
            edge_results_.erase(edge_results_.begin());
        }
    }
    
    std::cout << "🤖 [DataProcessor] Edge analysis completed for " << packet.sensor_id 
              << " (trend slope: " << slope << ")" << std::endl;
}

void DataProcessor::aggregate_and_forward() {
    std::cout << "📊 [DataProcessor] Performing data aggregation..." << std::endl;
    
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Aggregate data for each sensor
    for (const auto& pair : sensor_history_) {
        const std::string& sensor_id = pair.first;
        const std::vector<SensorDataPacket>& history = pair.second;
        
        if (history.empty()) {
            continue;
        }
        
        // Get recent data (last aggregation window)
        auto cutoff_time = std::chrono::steady_clock::now() - 
                          std::chrono::seconds(config_.aggregation_window_seconds);
        
        std::vector<SensorDataPacket> recent_data;
        for (const auto& packet : history) {
            if (packet.timestamp >= cutoff_time) {
                recent_data.push_back(packet);
            }
        }
        
        if (recent_data.empty()) {
            continue;
        }
        
        // Create aggregated message
        std::string aggregated_message = format_aggregated_data(recent_data);
        
        // Send to MQTT
        if (mqtt_callback_) {
            std::string topic = config_.mqtt_base_topic + "/sensors/" + sensor_id + "/aggregated";
            mqtt_callback_(topic, aggregated_message);
        }
        
        // Send to WebSocket
        if (websocket_callback_) {
            websocket_callback_(aggregated_message);
        }
    }
    
    std::cout << "📊 [DataProcessor] Aggregation completed for " 
              << sensor_history_.size() << " sensors" << std::endl;
}

std::string DataProcessor::format_mqtt_message(const SensorDataPacket& packet) {
    std::stringstream ss;
    ss << "{"
       << "\"sensor_id\":\"" << packet.sensor_id << "\","
       << "\"location\":\"" << packet.location << "\","
       << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
              packet.timestamp.time_since_epoch()).count() << ","
       << "\"temperature\":" << std::fixed << std::setprecision(2) << packet.temperature_celsius << ","
       << "\"humidity\":" << packet.humidity_percent << ","
       << "\"pressure\":" << packet.pressure_hpa << ","
       << "\"supply_voltage\":" << packet.supply_voltage << ","
       << "\"sensor_status\":" << static_cast<int>(packet.sensor_status) << ","
       << "\"interface\":\"" << (packet.interface_used == CommInterface::UART_INTERFACE ? "UART" :
                                packet.interface_used == CommInterface::SPI_INTERFACE ? "SPI" :
                                packet.interface_used == CommInterface::I2C_INTERFACE ? "I2C" : "UNKNOWN") << "\","
       << "\"signal_strength\":" << packet.signal_strength << ","
       << "\"data_confidence\":" << packet.data_confidence << ","
       << "\"gateway_id\":\"" << config_.gateway_id << "\""
       << "}";
    
    return ss.str();
}

std::string DataProcessor::format_websocket_message(const SensorDataPacket& packet) {
    std::stringstream ss;
    ss << "{"
       << "\"type\":\"sensor_data\","
       << "\"sensor_id\":\"" << packet.sensor_id << "\","
       << "\"location\":\"" << packet.location << "\","
       << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
              packet.timestamp.time_since_epoch()).count() << ","
       << "\"temperature\":" << std::fixed << std::setprecision(2) << packet.temperature_celsius << ","
       << "\"humidity\":" << packet.humidity_percent << ","
       << "\"pressure\":" << packet.pressure_hpa << ","
       << "\"supply_voltage\":" << packet.supply_voltage << ","
       << "\"gateway_id\":\"" << config_.gateway_id << "\","
       << "\"interface\":\"" << (packet.interface_used == CommInterface::UART_INTERFACE ? "UART" :
                                packet.interface_used == CommInterface::SPI_INTERFACE ? "SPI" :
                                packet.interface_used == CommInterface::I2C_INTERFACE ? "I2C" : "UNKNOWN") << "\""
       << "}";
    
    return ss.str();
}

std::string DataProcessor::format_aggregated_data(const std::vector<SensorDataPacket>& packets) {
    if (packets.empty()) {
        return "{}";
    }
    
    // Calculate aggregated statistics
    float sum_temp = 0.0f, sum_hum = 0.0f, sum_press = 0.0f;
    float min_temp = packets[0].temperature_celsius;
    float max_temp = packets[0].temperature_celsius;
    int valid_count = 0;
    
    for (const auto& packet : packets) {
        if (packet.is_valid) {
            sum_temp += packet.temperature_celsius;
            sum_hum += packet.humidity_percent;
            sum_press += packet.pressure_hpa;
            min_temp = std::min(min_temp, packet.temperature_celsius);
            max_temp = std::max(max_temp, packet.temperature_celsius);
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        return "{}";
    }
    
    float avg_temp = sum_temp / valid_count;
    float avg_hum = sum_hum / valid_count;
    float avg_press = sum_press / valid_count;
    
    std::stringstream ss;
    ss << "{"
       << "\"type\":\"aggregated_data\","
       << "\"sensor_id\":\"" << packets[0].sensor_id << "\","
       << "\"location\":\"" << packets[0].location << "\","
       << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count() << ","
       << "\"window_seconds\":" << config_.aggregation_window_seconds << ","
       << "\"sample_count\":" << packets.size() << ","
       << "\"valid_count\":" << valid_count << ","
       << "\"temperature\":{"
       << "\"avg\":" << std::fixed << std::setprecision(2) << avg_temp << ","
       << "\"min\":" << min_temp << ","
       << "\"max\":" << max_temp
       << "},"
       << "\"humidity\":{"
       << "\"avg\":" << avg_hum
       << "},"
       << "\"pressure\":{"
       << "\"avg\":" << avg_press
       << "},"
       << "\"gateway_id\":\"" << config_.gateway_id << "\""
       << "}";
    
    return ss.str();
}

} // namespace rpi4_gateway 