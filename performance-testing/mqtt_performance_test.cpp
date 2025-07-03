#include "../thermal-monitoring/ThermalIsolationTracker.h"
#include <mosquitto.h>
#include <jsoncpp/json/json.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

using namespace thermal_monitoring;

class MQTTPerformanceTest {
private:
    struct mosquitto* mosq_;
    std::unique_ptr<ThermalIsolationTracker> thermal_tracker_;
    bool running_;
    std::string client_id_;
    std::string broker_host_;
    int broker_port_;
    
    // Performance metrics
    std::atomic<int> messages_sent_{0};
    std::atomic<int> messages_received_{0};
    std::atomic<int> alerts_generated_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point test_start_time_;
    
    // Latency tracking
    std::mutex latency_mutex_;
    std::vector<double> latencies_;
    
    // System monitoring
    struct SystemStats {
        double cpu_usage;
        size_t memory_usage_mb;
        double throughput_msg_per_sec;
        double avg_latency_ms;
        int total_messages;
        int total_alerts;
    };
    
    std::vector<SystemStats> system_stats_;
    std::mutex stats_mutex_;
    
    // Test configuration
    int num_sensors_;
    int test_duration_seconds_;
    int message_interval_ms_;
    bool enable_system_monitoring_;
    
    // Sensor simulation
    std::vector<std::string> sensor_ids_;
    std::vector<std::string> locations_;
    std::mt19937 rng_;

public:
    MQTTPerformanceTest(int num_sensors = 10, 
                       int test_duration = 60,
                       int message_interval = 100,
                       const std::string& client_id = "mqtt_perf_test",
                       const std::string& broker_host = "localhost",
                       int broker_port = 1883)
        : mosq_(nullptr), running_(false), client_id_(client_id),
          broker_host_(broker_host), broker_port_(broker_port),
          num_sensors_(num_sensors), test_duration_seconds_(test_duration),
          message_interval_ms_(message_interval), enable_system_monitoring_(true),
          rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
        
        start_time_ = std::chrono::steady_clock::now();
        mosquitto_lib_init();
        
        // Initialize sensor IDs and locations
        for (int i = 1; i <= num_sensors; ++i) {
            sensor_ids_.push_back("sensor_" + std::to_string(i));
        }
        
        locations_ = {
            "Living Room", "Kitchen", "Bedroom 1", "Bedroom 2", "Bathroom",
            "Dining Room", "Study", "Basement", "Attic", "Garage"
        };
    }
    
    ~MQTTPerformanceTest() {
        Stop();
        if (mosq_) {
            mosquitto_destroy(mosq_);
        }
        mosquitto_lib_cleanup();
    }
    
    bool Start() {
        std::cout << "🚀 Starting MQTT Performance Test" << std::endl;
        std::cout << "   Sensors: " << num_sensors_ << std::endl;
        std::cout << "   Duration: " << test_duration_seconds_ << " seconds" << std::endl;
        std::cout << "   Message Interval: " << message_interval_ms_ << "ms" << std::endl;
        
        // Initialize thermal monitoring
        ThermalConfig thermal_config;
        thermal_config.temp_min = 18.0f;
        thermal_config.temp_max = 27.0f;
        thermal_config.humidity_max = 65.0f;
        thermal_config.temp_rate_limit = 2.0f;
        thermal_config.sensor_timeout_minutes = 5;
        
        thermal_tracker_ = std::make_unique<ThermalIsolationTracker>(thermal_config);
        
        // Set alert callback
        thermal_tracker_->set_alert_callback([this](const Alert& alert) {
            alerts_generated_++;
            if (alerts_generated_ <= 5) {
                std::cout << "🚨 Alert: [type=" << static_cast<int>(alert.alert_type) << "] for " << alert.sensor_id 
                          << " - " << alert.message << std::endl;
            }
        });
        
        // Initialize MQTT
        mosq_ = mosquitto_new(client_id_.c_str(), true, this);
        if (!mosq_) {
            std::cerr << "❌ Failed to create mosquitto instance" << std::endl;
            return false;
        }
        
        // Set callbacks
        mosquitto_connect_callback_set(mosq_, OnConnect);
        mosquitto_disconnect_callback_set(mosq_, OnDisconnect);
        mosquitto_message_callback_set(mosq_, OnMessage);
        
        // Connect to broker
        if (mosquitto_connect(mosq_, broker_host_.c_str(), broker_port_, 60) != MOSQ_ERR_SUCCESS) {
            std::cerr << "❌ Failed to connect to MQTT broker" << std::endl;
            return false;
        }
        
        // Start thermal monitoring
        if (!thermal_tracker_->start()) {
            std::cerr << "❌ Failed to start thermal monitoring" << std::endl;
            return false;
        }
        
        running_ = true;
        test_start_time_ = std::chrono::steady_clock::now();
        
        std::cout << "✅ MQTT Performance Test started" << std::endl;
        return true;
    }
    
    void Stop() {
        if (!running_) return;
        
        running_ = false;
        
        if (thermal_tracker_) {
            thermal_tracker_->stop();
        }
        
        if (mosq_) {
            mosquitto_disconnect(mosq_);
        }
        
        std::cout << "🛑 MQTT Performance Test stopped" << std::endl;
    }
    
    void RunTest() {
        if (!running_) return;
        
        std::cout << "🏃 Running performance test..." << std::endl;
        
        // Subscribe to sensor data
        mosquitto_subscribe(mosq_, nullptr, "sensors/+/data", 1);
        
        // Start system monitoring thread
        std::thread monitor_thread([this]() {
            MonitorSystemResources();
        });
        
        // Start sensor simulation thread
        std::thread sensor_thread([this]() {
            SimulateSensors();
        });
        
        // Run MQTT loop
        auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(test_duration_seconds_);
        
        while (std::chrono::steady_clock::now() < end_time && running_) {
            int result = mosquitto_loop(mosq_, 100, 1);
            if (result != MOSQ_ERR_SUCCESS) {
                std::cerr << "❌ MQTT loop error: " << mosquitto_strerror(result) << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        // Wait for threads to finish
        if (sensor_thread.joinable()) sensor_thread.join();
        if (monitor_thread.joinable()) monitor_thread.join();
        
        PrintFinalResults();
        SaveResultsToFile();
    }
    
private:
    void SimulateSensors() {
        std::cout << "🔄 Simulating " << num_sensors_ << " sensors..." << std::endl;
        
        auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(test_duration_seconds_);
        
        while (std::chrono::steady_clock::now() < end_time && running_) {
            for (int i = 0; i < num_sensors_; ++i) {
                SimulateSensorReading(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(message_interval_ms_ / num_sensors_));
            }
        }
    }
    
    void SimulateSensorReading(int sensor_index) {
        auto send_time = std::chrono::steady_clock::now();
        
        std::string sensor_id = sensor_ids_[sensor_index];
        std::string location = locations_[sensor_index % locations_.size()];
        
        // Generate realistic sensor data with some variation
        double base_temp = 20.0 + (sensor_index % 3) * 2.0;
        double base_humidity = 40.0 + (sensor_index % 4) * 5.0;
        
        std::uniform_real_distribution<> temp_dist(base_temp - 3.0, base_temp + 8.0);
        std::uniform_real_distribution<> humidity_dist(base_humidity - 15.0, base_humidity + 20.0);
        
        double temperature = temp_dist(rng_);
        double humidity = humidity_dist(rng_);
        
        // Process in thermal tracker
        thermal_tracker_->process_sensor_data(sensor_id, temperature, humidity, location);
        
        // Publish to MQTT
        PublishSensorData(sensor_id, temperature, humidity, location, send_time);
    }
    
    void PublishSensorData(const std::string& sensor_id, double temperature, double humidity, 
                          const std::string& location, std::chrono::steady_clock::time_point send_time) {
        Json::Value data;
        data["sensor_id"] = sensor_id;
        data["temperature"] = temperature;
        data["humidity"] = humidity;
        data["location"] = location;
        data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        data["send_time"] = std::chrono::duration_cast<std::chrono::microseconds>(
            send_time.time_since_epoch()).count();
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string payload = Json::writeString(builder, data);
        
        std::string topic = "sensors/" + sensor_id + "/data";
        
        if (mosquitto_publish(mosq_, nullptr, topic.c_str(), payload.length(), payload.c_str(), 1, false) == MOSQ_ERR_SUCCESS) {
            messages_sent_++;
        }
    }
    
    void MonitorSystemResources() {
        std::cout << "📊 Starting system resource monitoring..." << std::endl;
        
        auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(test_duration_seconds_);
        
        while (std::chrono::steady_clock::now() < end_time && running_) {
            SystemStats stats = GetCurrentSystemStats();
            
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                system_stats_.push_back(stats);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    SystemStats GetCurrentSystemStats() {
        SystemStats stats;
        
        // Get memory usage
        std::ifstream status_file("/proc/self/status");
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                size_t pos = line.find_first_of("0123456789");
                if (pos != std::string::npos) {
                    stats.memory_usage_mb = std::stoul(line.substr(pos)) / 1024;
                }
                break;
            }
        }
        
        // Calculate throughput and latency
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - test_start_time_);
        double duration_sec = duration.count() / 1000.0;
        
        stats.throughput_msg_per_sec = (messages_sent_.load() / duration_sec);
        stats.total_messages = messages_sent_.load();
        stats.total_alerts = alerts_generated_.load();
        
        // Calculate average latency
        {
            std::lock_guard<std::mutex> lock(latency_mutex_);
            if (!latencies_.empty()) {
                double sum = 0.0;
                for (double latency : latencies_) {
                    sum += latency;
                }
                stats.avg_latency_ms = sum / latencies_.size();
            } else {
                stats.avg_latency_ms = 0.0;
            }
        }
        
        // Simple CPU usage estimation (this is a simplified approach)
        static auto last_time = std::chrono::steady_clock::now();
        static int last_messages = 0;
        
        auto current_time = std::chrono::steady_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time);
        int current_messages = messages_sent_.load();
        int message_diff = current_messages - last_messages;
        
        if (time_diff.count() > 0) {
            // Estimate CPU based on message processing rate
            stats.cpu_usage = std::min(100.0, (message_diff * 0.1) / (time_diff.count() / 1000.0));
        } else {
            stats.cpu_usage = 0.0;
        }
        
        last_time = current_time;
        last_messages = current_messages;
        
        return stats;
    }
    
    void PrintFinalResults() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - test_start_time_);
        double duration_sec = duration.count() / 1000.0;
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "📊 MQTT PERFORMANCE TEST RESULTS" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        std::cout << "Test Configuration:" << std::endl;
        std::cout << "   Sensors: " << num_sensors_ << std::endl;
        std::cout << "   Duration: " << std::fixed << std::setprecision(2) << duration_sec << "s" << std::endl;
        std::cout << "   Message Interval: " << message_interval_ms_ << "ms" << std::endl;
        
        std::cout << "\nPerformance Metrics:" << std::endl;
        std::cout << "   Messages Sent: " << messages_sent_.load() << std::endl;
        std::cout << "   Messages Received: " << messages_received_.load() << std::endl;
        std::cout << "   Alerts Generated: " << alerts_generated_.load() << std::endl;
        std::cout << "   Throughput: " << std::fixed << std::setprecision(2) 
                  << (messages_sent_.load() / duration_sec) << " msg/sec" << std::endl;
        
        // Calculate average system stats
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (!system_stats_.empty()) {
                double avg_cpu = 0.0, avg_memory = 0.0, avg_latency = 0.0;
                for (const auto& stat : system_stats_) {
                    avg_cpu += stat.cpu_usage;
                    avg_memory += stat.memory_usage_mb;
                    avg_latency += stat.avg_latency_ms;
                }
                avg_cpu /= system_stats_.size();
                avg_memory /= system_stats_.size();
                avg_latency /= system_stats_.size();
                
                std::cout << "   Average CPU Usage: " << std::fixed << std::setprecision(1) << avg_cpu << "%" << std::endl;
                std::cout << "   Average Memory Usage: " << std::fixed << std::setprecision(1) << avg_memory << " MB" << std::endl;
                std::cout << "   Average Latency: " << std::fixed << std::setprecision(2) << avg_latency << " ms" << std::endl;
            }
        }
        
        // Get thermal monitoring stats
        auto sensors = thermal_tracker_->get_all_sensors();
        auto alerts = thermal_tracker_->get_recent_alerts(10);
        
        std::cout << "\nThermal Monitoring:" << std::endl;
        std::cout << "   Active Sensors: " << sensors.size() << std::endl;
        std::cout << "   Recent Alerts: " << alerts.size() << std::endl;
        
        std::cout << std::string(60, '=') << std::endl;
    }
    
    void SaveResultsToFile() {
        std::ofstream file("mqtt_performance_results.txt");
        if (!file.is_open()) {
            std::cerr << "❌ Could not open results file" << std::endl;
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - test_start_time_);
        double duration_sec = duration.count() / 1000.0;
        
        file << "MQTT Performance Test Results" << std::endl;
        file << "=============================" << std::endl;
        file << "Timestamp: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
        file << "Sensors: " << num_sensors_ << std::endl;
        file << "Duration: " << std::fixed << std::setprecision(2) << duration_sec << "s" << std::endl;
        file << "Message Interval: " << message_interval_ms_ << "ms" << std::endl;
        file << "Messages Sent: " << messages_sent_.load() << std::endl;
        file << "Messages Received: " << messages_received_.load() << std::endl;
        file << "Alerts Generated: " << alerts_generated_.load() << std::endl;
        file << "Throughput: " << std::fixed << std::setprecision(2) 
             << (messages_sent_.load() / duration_sec) << " msg/sec" << std::endl;
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (!system_stats_.empty()) {
                file << "\nSystem Statistics:" << std::endl;
                for (size_t i = 0; i < system_stats_.size(); ++i) {
                    const auto& stat = system_stats_[i];
                    file << "Sample " << (i + 1) << ": CPU=" << std::fixed << std::setprecision(1) 
                         << stat.cpu_usage << "%, Memory=" << stat.memory_usage_mb 
                         << "MB, Latency=" << std::setprecision(2) << stat.avg_latency_ms << "ms" << std::endl;
                }
            }
        }
        
        file.close();
        std::cout << "💾 Results saved to mqtt_performance_results.txt" << std::endl;
    }
    
    void ProcessIncomingSensorData(const std::string& topic, const std::string& payload) {
        messages_received_++;
        
        // Parse JSON to calculate latency
        Json::Value data;
        Json::Reader reader;
        if (reader.parse(payload, data)) {
            if (data.isMember("send_time")) {
                auto send_time = std::chrono::microseconds(data["send_time"].asInt64());
                auto receive_time = std::chrono::steady_clock::now().time_since_epoch();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(receive_time - send_time);
                
                {
                    std::lock_guard<std::mutex> lock(latency_mutex_);
                    latencies_.push_back(latency.count() / 1000.0); // Convert to ms
                    
                    // Keep only last 1000 measurements to avoid memory issues
                    if (latencies_.size() > 1000) {
                        latencies_.erase(latencies_.begin());
                    }
                }
            }
        }
    }
    
    // Static MQTT callbacks
    static void OnConnect(struct mosquitto* mosq, void* userdata, int result) {
        MQTTPerformanceTest* test = static_cast<MQTTPerformanceTest*>(userdata);
        if (result == 0) {
            std::cout << "✅ Connected to MQTT broker" << std::endl;
        } else {
            std::cerr << "❌ Failed to connect: " << mosquitto_connack_string(result) << std::endl;
        }
    }
    
    static void OnDisconnect(struct mosquitto* mosq, void* userdata, int result) {
        std::cout << "🔌 Disconnected from MQTT broker" << std::endl;
    }
    
    static void OnMessage(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* msg) {
        MQTTPerformanceTest* test = static_cast<MQTTPerformanceTest*>(userdata);
        std::string topic = msg->topic;
        std::string payload(static_cast<char*>(msg->payload), msg->payloadlen);
        test->ProcessIncomingSensorData(topic, payload);
    }
};

int main(int argc, char* argv[]) {
    std::cout << "🚀 MQTT Performance Test with 10 Sensors" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Parse command line arguments
    int num_sensors = 10;
    int test_duration = 60;
    int message_interval = 100;
    
    if (argc > 1) num_sensors = std::stoi(argv[1]);
    if (argc > 2) test_duration = std::stoi(argv[2]);
    if (argc > 3) message_interval = std::stoi(argv[3]);
    
    MQTTPerformanceTest test(num_sensors, test_duration, message_interval);
    
    if (!test.Start()) {
        std::cerr << "❌ Failed to start performance test" << std::endl;
        return 1;
    }
    
    test.RunTest();
    
    std::cout << "✅ Performance test completed!" << std::endl;
    return 0;
} 