#include "../../thermal-monitoring/ThermalIsolationTracker.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>

using namespace thermal_monitoring;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void simulate_sensor_data(ThermalIsolationTracker& tracker) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> temp_dist(20.0f, 30.0f);
    std::uniform_real_distribution<float> humidity_dist(40.0f, 70.0f);
    std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);
    
    // Simulate different sensor scenarios
    struct SensorScenario {
        std::string id;
        std::string location;
        float base_temp;
        float base_humidity;
        std::string scenario_type;
    };
    
    std::vector<SensorScenario> sensors = {
        {"sensor_001", "Living Room", 22.0f, 50.0f, "normal"},
        {"sensor_002", "Kitchen", 25.0f, 55.0f, "temp_rising"},
        {"sensor_003", "Bedroom", 20.0f, 45.0f, "humidity_high"},
        {"sensor_004", "Basement", 15.0f, 65.0f, "temp_low"},
        {"sensor_005", "Attic", 28.0f, 40.0f, "temp_high"}
    };
    
    print_separator("Starting Sensor Data Simulation");
    
    for (int cycle = 0; cycle < 10; cycle++) {
        std::cout << "\n--- Simulation Cycle " << (cycle + 1) << " ---" << std::endl;
        
        for (auto& sensor : sensors) {
            float temp = sensor.base_temp;
            float humidity = sensor.base_humidity;
            
            // Apply scenario-specific changes
            if (sensor.scenario_type == "temp_rising" && cycle > 5) {
                temp += (cycle - 5) * 1.5f; // Rising temperature
            } else if (sensor.scenario_type == "humidity_high" && cycle > 3) {
                humidity += (cycle - 3) * 3.0f; // Rising humidity
            } else if (sensor.scenario_type == "temp_low") {
                temp -= std::abs(noise_dist(gen)) * 2.0f; // Consistently low temp
            } else if (sensor.scenario_type == "temp_high" && cycle > 2) {
                temp += std::abs(noise_dist(gen)) * 2.0f; // Consistently high temp
            }
            
            // Add some random noise
            temp += noise_dist(gen) * 0.5f;
            humidity += noise_dist(gen) * 2.0f;
            
            // Process the sensor data
            tracker.process_sensor_data(sensor.id, temp, humidity, sensor.location);
        }
        
        // Wait before next cycle
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}

void test_message_parsing() {
    print_separator("Testing Message Parsing");
    
    // Test different message formats
    std::vector<std::pair<std::string, std::string>> test_messages = {
        {"sensors/sensor_001/data", "{\"temperature\": 25.5, \"humidity\": 60.2, \"location\": \"room1\"}"},
        {"sensors/sensor_002/temperature", "23.7"},
        {"sensors/sensor_003/humidity", "58.5"},
        {"sensors/sensor_004/data", "{\"temperature\": 19.2, \"humidity\": 45.8}"},
        {"invalid/topic", "should not parse"},
        {"sensors/sensor_005/data", "invalid json format"}
    };
    
    for (const auto& test : test_messages) {
        std::cout << "\nTesting: " << test.first << " -> " << test.second << std::endl;
        
        auto result = parse_sensor_message(test.first, test.second);
        if (result) {
            std::cout << "  ✅ Parsed: ID=" << result->sensor_id 
                      << ", Temp=" << result->temperature 
                      << "°C, Humidity=" << result->humidity 
                      << "%, Location=" << result->location << std::endl;
        } else {
            std::cout << "  ❌ Failed to parse" << std::endl;
        }
    }
}

void test_threshold_alerts() {
    print_separator("Testing Threshold Alerts");
    
    // Create configuration with tight thresholds for testing
    ThermalConfig config;
    config.temp_min = 18.0f;
    config.temp_max = 26.0f;
    config.humidity_max = 60.0f;
    config.temp_rate_limit = 1.5f;
    config.sensor_timeout_minutes = 1;
    config.alert_throttle_minutes = 1;
    
    // Add sensor locations
    config.sensor_locations["test_sensor_1"] = "Test Room 1";
    config.sensor_locations["test_sensor_2"] = "Test Room 2";
    
    ThermalIsolationTracker tracker(config);
    
    // Set up alert handler
    int alert_count = 0;
    tracker.set_alert_callback([&alert_count](const Alert& alert) {
        alert_count++;
        std::cout << "🚨 ALERT #" << alert_count << ": " << alert.message << std::endl;
    });
    
    tracker.start();
    
    std::cout << "Testing various threshold violations..." << std::endl;
    
    // Test temperature too low
    tracker.process_sensor_data("test_sensor_1", 15.0f, 45.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Test temperature too high
    tracker.process_sensor_data("test_sensor_2", 30.0f, 55.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Test humidity too high
    tracker.process_sensor_data("test_sensor_1", 22.0f, 70.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Test rapid temperature change
    tracker.process_sensor_data("test_sensor_2", 22.0f, 50.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    tracker.process_sensor_data("test_sensor_2", 27.0f, 50.0f); // Rapid rise
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "Total alerts generated: " << alert_count << std::endl;
    
    tracker.stop();
}

void print_system_stats(const ThermalIsolationTracker& tracker) {
    print_separator("System Statistics");
    
    auto sensors = tracker.get_all_sensors();
    std::cout << "Active Sensors: " << sensors.size() << std::endl;
    
    for (const auto& sensor : sensors) {
        std::cout << "\n📊 Sensor: " << sensor.sensor_id << std::endl;
        std::cout << "   Location: " << sensor.location << std::endl;
        std::cout << "   Temperature: " << sensor.temperature << "°C" << std::endl;
        std::cout << "   Humidity: " << sensor.humidity << "%" << std::endl;
        std::cout << "   Rate: " << sensor.temp_rate << "°C/min" << std::endl;
        std::cout << "   Active: " << (sensor.is_active ? "Yes" : "No") << std::endl;
        std::cout << "   History: " << sensor.temperature_history.size() << " readings" << std::endl;
    }
    
    auto alerts = tracker.get_recent_alerts(5);
    std::cout << "\nRecent Alerts (" << alerts.size() << "):" << std::endl;
    for (const auto& alert : alerts) {
        std::cout << "  • [" << alert.sensor_id << "] " << alert.message << std::endl;
    }
}

int main() {
    std::cout << "🌡️  Thermal Isolation Tracker Test Program" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    try {
        // Test 1: Message parsing
        test_message_parsing();
        
        // Test 2: Threshold alerts
        test_threshold_alerts();
        
        // Test 3: Full system simulation
        print_separator("Full System Simulation");
        
        ThermalConfig config;
        config.temp_min = 18.0f;
        config.temp_max = 27.0f;
        config.humidity_max = 65.0f;
        config.temp_rate_limit = 2.0f;
        
        // Configure sensor locations
        config.sensor_locations["sensor_001"] = "Living Room";
        config.sensor_locations["sensor_002"] = "Kitchen";
        config.sensor_locations["sensor_003"] = "Bedroom";
        config.sensor_locations["sensor_004"] = "Basement";
        config.sensor_locations["sensor_005"] = "Attic";
        
        ThermalIsolationTracker tracker(config);
        
        // Set up alert handler
        int total_alerts = 0;
        tracker.set_alert_callback([&total_alerts](const Alert& alert) {
            total_alerts++;
            std::cout << "🚨 SYSTEM ALERT: " << alert.message 
                      << " (Location: " << alert.location << ")" << std::endl;
        });
        
        tracker.start();
        
        // Run simulation
        simulate_sensor_data(tracker);
        
        // Show final statistics
        print_system_stats(tracker);
        
        std::cout << "\nTotal system alerts: " << total_alerts << std::endl;
        
        tracker.stop();
        
        print_separator("Test Complete");
        std::cout << "✅ All tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 