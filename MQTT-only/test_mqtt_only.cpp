#include "mqtt_only_bridge.h"
#include "../ThermalIsolationTracker.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <random>
#include <vector>
#include <atomic>

using namespace thermal_monitoring;

class MQTTOnlyTester {
private:
    std::unique_ptr<MQTTOnlyBridge> bridge_;
    std::atomic<int> messages_received_{0};
    std::atomic<int> alerts_received_{0};
    std::atomic<int> commands_received_{0};
    std::vector<std::string> received_messages_;
    std::mutex received_mutex_;
    
public:
    MQTTOnlyTester() = default;
    
    bool InitializeBridge(const MQTTOnlyBridge::Config& config) {
        bridge_ = std::make_unique<MQTTOnlyBridge>(config);
        
        // Set up callbacks
        bridge_->SetMessageCallback([this](const std::string& topic, const std::string& payload, int qos) {
            std::lock_guard<std::mutex> lock(received_mutex_);
            received_messages_.push_back(topic + "|" + payload);
            messages_received_++;
            std::cout << "📩 Received message on topic '" << topic << "': " << payload << std::endl;
        });
        
        bridge_->SetAlertCallback([this](const Alert& alert) {
            alerts_received_++;
            std::cout << "🚨 Alert received from sensor " << alert.sensor_id << ": " << alert.message << std::endl;
        });
        
        bridge_->SetCommandCallback([this](const std::string& command, const Json::Value& params) {
            commands_received_++;
            std::cout << "📋 Command received: " << command << std::endl;
            return "Command executed successfully";
        });
        
        bridge_->SetConnectionCallback([](MQTTOnlyBridge::ConnectionStatus status, const std::string& message) {
            std::cout << "🔗 Connection status: " << static_cast<int>(status) << " - " << message << std::endl;
        });
        
        return bridge_->Start();
    }
    
    void RunBasicConnectivityTest() {
        std::cout << "\n=== Running Basic Connectivity Test ===" << std::endl;
        
        MQTTOnlyBridge::Config config = MQTTOnlyUtils::CreateDefaultConfig();
        config.client_id = "test_mqtt_basic";
        
        // Test broker connection validation
        bool broker_available = MQTTOnlyUtils::ValidateBrokerConnection(config.broker_host, config.broker_port);
        if (!broker_available) {
            std::cout << "❌ MQTT broker not available, skipping connectivity tests" << std::endl;
            return;
        }
        
        std::cout << "✅ MQTT broker is available" << std::endl;
        
        // Initialize bridge
        assert(InitializeBridge(config));
        std::cout << "✅ Bridge initialized and started" << std::endl;
        
        // Wait for connection
        std::this_thread::sleep_for(std::chrono::seconds(2));
        assert(bridge_->IsConnected());
        std::cout << "✅ Connected to MQTT broker" << std::endl;
        
        // Test basic subscription
        assert(bridge_->SubscribeToSensorData());
        std::cout << "✅ Subscribed to sensor data" << std::endl;
        
        // Test basic publishing
        Json::Value sensor_data;
        sensor_data["temperature"] = 22.5;
        sensor_data["humidity"] = 45.0;
        sensor_data["location"] = "test_room";
        
        assert(bridge_->PublishSensorData("test_sensor_01", sensor_data));
        std::cout << "✅ Published sensor data" << std::endl;
        
        // Wait for message processing
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Check metrics
        auto metrics = bridge_->GetMetrics();
        assert(metrics.messages_published > 0);
        assert(metrics.is_connected.load());
        std::cout << "✅ Metrics validation passed" << std::endl;
        
        bridge_->Stop();
        std::cout << "✅ Basic connectivity test completed" << std::endl;
    }
    
    void RunMultiClientTest() {
        std::cout << "\n=== Running Multi-Client Communication Test ===" << std::endl;
        
        MQTTOnlyBridge::Config config = MQTTOnlyUtils::CreateDefaultConfig();
        config.client_id = "test_mqtt_multi";
        config.worker_thread_count = 6;
        
        if (!MQTTOnlyUtils::ValidateBrokerConnection(config.broker_host, config.broker_port)) {
            std::cout << "❌ MQTT broker not available, skipping multi-client test" << std::endl;
            return;
        }
        
        assert(InitializeBridge(config));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Subscribe to multiple topics
        assert(bridge_->SubscribeToSensorData());
        assert(bridge_->SubscribeToAlerts());
        assert(bridge_->SubscribeToCommands());
        std::cout << "✅ Subscribed to multiple topic types" << std::endl;
        
        // Simulate multiple sensors
        const int sensor_count = 5;
        const int messages_per_sensor = 3;
        
        for (int i = 0; i < sensor_count; ++i) {
            for (int j = 0; j < messages_per_sensor; ++j) {
                Json::Value sensor_data;
                sensor_data["temperature"] = 20.0 + i * 2.0 + j * 0.5;
                sensor_data["humidity"] = 40.0 + i * 5.0 + j * 2.0;
                sensor_data["location"] = "room_" + std::to_string(i);
                
                std::string sensor_id = "multi_sensor_" + std::to_string(i);
                assert(bridge_->PublishSensorData(sensor_id, sensor_data));
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        std::cout << "✅ Published messages from " << sensor_count << " sensors" << std::endl;
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        auto metrics = bridge_->GetMetrics();
        assert(metrics.messages_published >= sensor_count * messages_per_sensor);
        assert(metrics.messages_processed > 0);
        std::cout << "✅ Multi-client communication test completed" << std::endl;
        
        bridge_->Stop();
    }
    
    void RunQoSLevelTest() {
        std::cout << "\n=== Running QoS Level Test ===" << std::endl;
        
        MQTTOnlyBridge::Config config = MQTTOnlyUtils::CreateDefaultConfig();
        config.client_id = "test_mqtt_qos";
        config.sensor_data_qos = 0;  // Fire-and-forget
        config.alerts_qos = 2;       // Exactly-once
        config.commands_qos = 1;     // At-least-once
        
        if (!MQTTOnlyUtils::ValidateBrokerConnection(config.broker_host, config.broker_port)) {
            std::cout << "❌ MQTT broker not available, skipping QoS test" << std::endl;
            return;
        }
        
        assert(InitializeBridge(config));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Test QoS 0 (sensor data)
        Json::Value sensor_data;
        sensor_data["temperature"] = 25.0;
        sensor_data["humidity"] = 50.0;
        assert(bridge_->PublishSensorData("qos_test_sensor", sensor_data));
        std::cout << "✅ Published QoS 0 message (sensor data)" << std::endl;
        
        // Test QoS 2 (alert)
        std::string alert_topic = "alerts/test_alert";
        std::string alert_payload = MQTTOnlyUtils::CreateAlertPayload("qos_test_sensor", "high_temp", "Temperature too high", 30.0);
        assert(bridge_->PublishMessage(alert_topic, alert_payload, 2));
        std::cout << "✅ Published QoS 2 message (alert)" << std::endl;
        
        // Test QoS 1 (command)
        std::string command_topic = "commands/test_device";
        Json::Value command_params;
        command_params["action"] = "set_threshold";
        command_params["value"] = 28.0;
        std::string command_payload = MQTTOnlyUtils::CreateCommandPayload("set_threshold", command_params);
        assert(bridge_->PublishMessage(command_topic, command_payload, 1));
        std::cout << "✅ Published QoS 1 message (command)" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto metrics = bridge_->GetMetrics();
        assert(metrics.messages_published >= 3);
        std::cout << "✅ QoS level test completed" << std::endl;
        
        bridge_->Stop();
    }
    
    void RunThermalIntegrationTest() {
        std::cout << "\n=== Running Thermal Integration Test ===" << std::endl;
        
        MQTTOnlyBridge::Config config = MQTTOnlyUtils::CreateDefaultConfig();
        config.client_id = "test_mqtt_thermal";
        config.enable_thermal_processing = true;
        
        if (!MQTTOnlyUtils::ValidateBrokerConnection(config.broker_host, config.broker_port)) {
            std::cout << "❌ MQTT broker not available, skipping thermal test" << std::endl;
            return;
        }
        
        assert(InitializeBridge(config));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Configure thermal monitoring
        ThermalConfig thermal_config;
        thermal_config.temp_min = 18.0f;
        thermal_config.temp_max = 26.0f;
        thermal_config.humidity_max = 60.0f;
        
        assert(bridge_->EnableThermalMonitoring(thermal_config));
        std::cout << "✅ Thermal monitoring enabled" << std::endl;
        
        // Subscribe to alerts
        assert(bridge_->SubscribeToAlerts());
        
        // Test normal temperature (should not trigger alert)
        Json::Value normal_data;
        normal_data["temperature"] = 22.0;
        normal_data["humidity"] = 45.0;
        assert(bridge_->PublishSensorData("thermal_test_sensor", normal_data));
        std::cout << "✅ Published normal temperature data" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Test high temperature (should trigger alert)
        Json::Value high_temp_data;
        high_temp_data["temperature"] = 30.0;  // Above threshold
        high_temp_data["humidity"] = 45.0;
        assert(bridge_->PublishSensorData("thermal_test_sensor", high_temp_data));
        std::cout << "✅ Published high temperature data" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Test high humidity (should trigger alert)
        Json::Value high_humidity_data;
        high_humidity_data["temperature"] = 22.0;
        high_humidity_data["humidity"] = 70.0;  // Above threshold
        assert(bridge_->PublishSensorData("thermal_test_sensor", high_humidity_data));
        std::cout << "✅ Published high humidity data" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        auto metrics = bridge_->GetMetrics();
        std::cout << "Thermal alerts generated: " << metrics.thermal_alerts_generated.load() << std::endl;
        assert(metrics.thermal_alerts_generated > 0);
        std::cout << "✅ Thermal integration test completed" << std::endl;
        
        bridge_->DisableThermalMonitoring();
        bridge_->Stop();
    }
    
    void RunPerformanceBenchmark() {
        std::cout << "\n=== Running Performance Benchmark ===" << std::endl;
        
        MQTTOnlyBridge::Config config = MQTTOnlyUtils::CreateHighThroughputConfig();
        config.client_id = "test_mqtt_performance";
        
        if (!MQTTOnlyUtils::ValidateBrokerConnection(config.broker_host, config.broker_port)) {
            std::cout << "❌ MQTT broker not available, skipping performance test" << std::endl;
            return;
        }
        
        assert(InitializeBridge(config));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Run performance test
        int duration = 10;  // seconds
        int target_rate = 1000;  // messages per second
        
        std::cout << "Starting " << duration << "s benchmark at " << target_rate << " msg/sec..." << std::endl;
        
        MQTTOnlyUtils::RunPerformanceTest(*bridge_, duration, target_rate);
        
        auto metrics = bridge_->GetMetrics();
        double throughput = bridge_->GetCurrentThroughput();
        
        std::cout << "Final throughput: " << throughput << " msg/sec" << std::endl;
        std::cout << "Average latency: " << bridge_->GetAverageLatency() << " ms" << std::endl;
        std::cout << "Queue size: " << bridge_->GetQueueSize() << std::endl;
        
        // Verify performance meets targets
        assert(throughput > target_rate * 0.8);  // At least 80% of target
        assert(metrics.messages_failed.load() < metrics.messages_published.load() * 0.01);  // Less than 1% failure
        
        std::cout << "✅ Performance benchmark completed" << std::endl;
        
        bridge_->Stop();
    }
    
    void RunStressTest() {
        std::cout << "\n=== Running Stress Test ===" << std::endl;
        
        MQTTOnlyBridge::Config config = MQTTOnlyUtils::CreateHighThroughputConfig();
        config.client_id = "test_mqtt_stress";
        config.worker_thread_count = 8;
        config.message_buffer_size = 50000;
        
        if (!MQTTOnlyUtils::ValidateBrokerConnection(config.broker_host, config.broker_port)) {
            std::cout << "❌ MQTT broker not available, skipping stress test" << std::endl;
            return;
        }
        
        assert(InitializeBridge(config));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Subscribe to sensor data
        assert(bridge_->SubscribeToSensorData());
        
        // Generate high load
        const int concurrent_sensors = 50;
        const int messages_per_sensor = 100;
        const int total_messages = concurrent_sensors * messages_per_sensor;
        
        std::cout << "Generating " << total_messages << " messages from " << concurrent_sensors << " sensors..." << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        auto start_metrics = bridge_->GetMetrics();
        
        // Create threads to simulate concurrent sensors
        std::vector<std::thread> sensor_threads;
        std::atomic<int> messages_sent{0};
        
        for (int sensor_id = 0; sensor_id < concurrent_sensors; ++sensor_id) {
            sensor_threads.emplace_back([this, sensor_id, messages_per_sensor, &messages_sent]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> temp_dist(15.0, 35.0);
                std::uniform_real_distribution<> humidity_dist(30.0, 80.0);
                
                for (int i = 0; i < messages_per_sensor; ++i) {
                    Json::Value sensor_data;
                    sensor_data["temperature"] = temp_dist(gen);
                    sensor_data["humidity"] = humidity_dist(gen);
                    sensor_data["location"] = "stress_room_" + std::to_string(sensor_id);
                    
                    std::string sensor_name = "stress_sensor_" + std::to_string(sensor_id);
                    if (bridge_->PublishSensorData(sensor_name, sensor_data)) {
                        messages_sent++;
                    }
                    
                    // Small delay to avoid overwhelming
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : sensor_threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Wait for processing to complete
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        auto end_metrics = bridge_->GetMetrics();
        
        uint64_t messages_published = end_metrics.messages_published - start_metrics.messages_published;
        uint64_t messages_processed = end_metrics.messages_processed - start_metrics.messages_processed;
        
        std::cout << "=== Stress Test Results ===" << std::endl;
        std::cout << "Duration: " << duration.count() << " ms" << std::endl;
        std::cout << "Messages sent: " << messages_sent.load() << std::endl;
        std::cout << "Messages published: " << messages_published << std::endl;
        std::cout << "Messages processed: " << messages_processed << std::endl;
        std::cout << "Throughput: " << MQTTOnlyUtils::CalculateThroughput(messages_published, duration) << " msg/sec" << std::endl;
        std::cout << "Max queue size: " << end_metrics.max_queue_size.load() << std::endl;
        std::cout << "Failed messages: " << end_metrics.messages_failed.load() << std::endl;
        
        // Verify system stability
        assert(messages_published > total_messages * 0.9);  // At least 90% published
        assert(end_metrics.messages_failed.load() < messages_published * 0.05);  // Less than 5% failure
        assert(bridge_->IsConnected());  // Still connected
        
        std::cout << "✅ Stress test completed" << std::endl;
        
        bridge_->Stop();
    }
    
    void RunReliabilityTest() {
        std::cout << "\n=== Running Reliability Test ===" << std::endl;
        
        MQTTOnlyBridge::Config config = MQTTOnlyUtils::CreateDefaultConfig();
        config.client_id = "test_mqtt_reliability";
        config.message_retry_count = 3;
        config.reconnect_delay = std::chrono::milliseconds(1000);
        
        if (!MQTTOnlyUtils::ValidateBrokerConnection(config.broker_host, config.broker_port)) {
            std::cout << "❌ MQTT broker not available, skipping reliability test" << std::endl;
            return;
        }
        
        assert(InitializeBridge(config));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Test message publishing reliability
        const int reliability_messages = 100;
        int successful_publishes = 0;
        
        for (int i = 0; i < reliability_messages; ++i) {
            Json::Value sensor_data;
            sensor_data["temperature"] = 20.0 + (i % 20);
            sensor_data["humidity"] = 40.0 + (i % 40);
            sensor_data["location"] = "reliability_room";
            
            std::string sensor_id = "reliability_sensor_" + std::to_string(i % 5);
            if (bridge_->PublishSensorData(sensor_id, sensor_data)) {
                successful_publishes++;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        auto metrics = bridge_->GetMetrics();
        double success_rate = (double)successful_publishes / reliability_messages * 100.0;
        
        std::cout << "Reliability test results:" << std::endl;
        std::cout << "Messages attempted: " << reliability_messages << std::endl;
        std::cout << "Successful publishes: " << successful_publishes << std::endl;
        std::cout << "Success rate: " << success_rate << "%" << std::endl;
        std::cout << "Failed messages: " << metrics.messages_failed.load() << std::endl;
        
        // Verify reliability (should be > 95%)
        assert(success_rate > 95.0);
        assert(bridge_->IsConnected());
        
        std::cout << "✅ Reliability test completed" << std::endl;
        
        bridge_->Stop();
    }
    
    void RunAllTests() {
        std::cout << "🚀 Starting MQTT-Only Bridge Test Suite" << std::endl;
        std::cout << "========================================" << std::endl;
        
        try {
            RunBasicConnectivityTest();
            RunMultiClientTest();
            RunQoSLevelTest();
            RunThermalIntegrationTest();
            RunPerformanceBenchmark();
            RunStressTest();
            RunReliabilityTest();
            
            std::cout << "\n🎉 All tests completed successfully!" << std::endl;
            std::cout << "========================================" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
            throw;
        }
    }
};

int main() {
    std::cout << "MQTT-Only Bridge Test Suite" << std::endl;
    std::cout << "===========================" << std::endl;
    
    try {
        MQTTOnlyTester tester;
        tester.RunAllTests();
        
        std::cout << "\n✅ All MQTT-Only Bridge tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test suite failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test suite failed with unknown exception" << std::endl;
        return 1;
    }
}
