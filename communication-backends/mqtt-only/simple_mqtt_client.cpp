#include "../../thermal-monitoring/ThermalIsolationTracker.h"
#include <mosquitto.h>
#include <jsoncpp/json/json.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <vector>

using namespace thermal_monitoring;

class SimpleMQTTClient {
private:
    struct mosquitto* mosq_;
    std::unique_ptr<ThermalIsolationTracker> thermal_tracker_;
    bool running_;
    std::string client_id_;
    std::string broker_host_;
    int broker_port_;
    
    // Performance metrics
    int messages_sent_ = 0;
    int messages_received_ = 0;
    std::chrono::steady_clock::time_point start_time_;

public:
    SimpleMQTTClient(const std::string& client_id = "mqtt_thermal_client", 
                     const std::string& broker_host = "localhost", 
                     int broker_port = 1883)
        : mosq_(nullptr), running_(false), client_id_(client_id), 
          broker_host_(broker_host), broker_port_(broker_port) {
        
        start_time_ = std::chrono::steady_clock::now();
        mosquitto_lib_init();
    }
    
    ~SimpleMQTTClient() {
        Stop();
        if (mosq_) {
            mosquitto_destroy(mosq_);
        }
        mosquitto_lib_cleanup();
    }
    
    bool Start() {
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
            PublishAlert(alert);
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
        std::cout << "✅ MQTT Thermal Client started" << std::endl;
        std::cout << "   Client ID: " << client_id_ << std::endl;
        std::cout << "   Broker: " << broker_host_ << ":" << broker_port_ << std::endl;
        
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
        
        std::cout << "🛑 MQTT Thermal Client stopped" << std::endl;
    }
    
    void Run() {
        if (!running_) return;
        
        std::cout << "🏃 Starting MQTT client loop..." << std::endl;
        
        // Subscribe to sensor data
        mosquitto_subscribe(mosq_, nullptr, "sensors/+/data", 1);
        
        while (running_) {
            int result = mosquitto_loop(mosq_, 100, 1);
            if (result != MOSQ_ERR_SUCCESS) {
                std::cerr << "❌ MQTT loop error: " << mosquitto_strerror(result) << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    
    void SimulateSensors(int duration_seconds = 30) {
        std::cout << "🔄 Simulating sensor data for " << duration_seconds << " seconds..." << std::endl;
        
        auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration_seconds);
        
        while (std::chrono::steady_clock::now() < end_time && running_) {
            // Simulate 5 sensors
            for (int i = 1; i <= 5; ++i) {
                SimulateSensorReading(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        PrintPerformanceStats();
    }
    
private:
    void SimulateSensorReading(int sensor_num) {
        // Generate realistic sensor data
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        std::string sensor_id = "sensor_" + std::to_string(sensor_num);
        
        // Different patterns for different sensors
        double base_temp = 20.0 + sensor_num * 2.0;
        double base_humidity = 40.0 + sensor_num * 5.0;
        
        std::uniform_real_distribution<> temp_dist(base_temp - 5.0, base_temp + 10.0);
        std::uniform_real_distribution<> humidity_dist(base_humidity - 10.0, base_humidity + 25.0);
        
        double temperature = temp_dist(gen);
        double humidity = humidity_dist(gen);
        
        std::string location = GetLocationForSensor(sensor_num);
        
        // Process in thermal tracker
        thermal_tracker_->process_sensor_data(sensor_id, temperature, humidity, location);
        
        // Publish to MQTT
        PublishSensorData(sensor_id, temperature, humidity, location);
    }
    
    std::string GetLocationForSensor(int sensor_num) {
        const std::vector<std::string> locations = {
            "Living Room", "Kitchen", "Bedroom", "Basement", "Attic"
        };
        return locations[sensor_num - 1];
    }
    
    void PublishSensorData(const std::string& sensor_id, double temperature, double humidity, const std::string& location) {
        Json::Value data;
        data["sensor_id"] = sensor_id;
        data["temperature"] = temperature;
        data["humidity"] = humidity;
        data["location"] = location;
        data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string payload = Json::writeString(builder, data);
        
        std::string topic = "sensors/" + sensor_id + "/data";
        
        if (mosquitto_publish(mosq_, nullptr, topic.c_str(), payload.length(), payload.c_str(), 1, false) == MOSQ_ERR_SUCCESS) {
            messages_sent_++;
        }
    }
    
    void PublishAlert(const Alert& alert) {
        Json::Value alert_data;
        alert_data["sensor_id"] = alert.sensor_id;
        alert_data["alert_type"] = alert.message;
        alert_data["temperature"] = alert.temperature;
        alert_data["humidity"] = alert.humidity;
        alert_data["location"] = alert.location;
        alert_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string payload = Json::writeString(builder, alert_data);
        
        std::string topic = "alerts/" + alert.sensor_id;
        
        if (mosquitto_publish(mosq_, nullptr, topic.c_str(), payload.length(), payload.c_str(), 2, false) == MOSQ_ERR_SUCCESS) {
            std::cout << "🚨 Published alert: " << alert.message << " for " << alert.sensor_id << std::endl;
        }
    }
    
    void PrintPerformanceStats() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        double duration_sec = duration.count() / 1000.0;
        
        std::cout << "\n📊 Performance Results:" << std::endl;
        std::cout << "Messages sent: " << messages_sent_ << std::endl;
        std::cout << "Messages received: " << messages_received_ << std::endl;
        std::cout << "Duration: " << std::fixed << std::setprecision(2) << duration_sec << "s" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) << (messages_sent_ / duration_sec) << " msg/sec" << std::endl;
        
        // Get thermal monitoring stats
        auto sensors = thermal_tracker_->get_all_sensors();
        auto alerts = thermal_tracker_->get_recent_alerts(10);
        
        std::cout << "Active sensors: " << sensors.size() << std::endl;
        std::cout << "Recent alerts: " << alerts.size() << std::endl;
    }
    
    // Static MQTT callbacks
    static void OnConnect(struct mosquitto* mosq, void* userdata, int result) {
        SimpleMQTTClient* client = static_cast<SimpleMQTTClient*>(userdata);
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
        SimpleMQTTClient* client = static_cast<SimpleMQTTClient*>(userdata);
        client->messages_received_++;
        
        std::string topic = msg->topic;
        std::string payload(static_cast<char*>(msg->payload), msg->payloadlen);
        
        // Process incoming sensor data if from other clients
        if (topic.find("sensors/") == 0 && topic.find("/data") != std::string::npos) {
            client->ProcessIncomingSensorData(topic, payload);
        }
    }
    
    void ProcessIncomingSensorData(const std::string& topic, const std::string& payload) {
        try {
            Json::Value data;
            Json::CharReaderBuilder reader_builder;
            std::string errors;
            std::stringstream ss(payload);
            
            if (Json::parseFromStream(reader_builder, ss, &data, &errors)) {
                std::string sensor_id = data.get("sensor_id", "").asString();
                double temperature = data.get("temperature", 0.0).asDouble();
                double humidity = data.get("humidity", 0.0).asDouble();
                std::string location = data.get("location", "").asString();
                
                // Don't process our own messages (basic loop prevention)
                if (topic.find(client_id_) == std::string::npos) {
                    thermal_tracker_->process_sensor_data(sensor_id, temperature, humidity, location);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error processing incoming sensor data: " << e.what() << std::endl;
        }
    }
};

int main() {
    std::cout << "🚀 MQTT-Only Thermal Monitoring Client" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    SimpleMQTTClient client("mqtt_thermal_test");
    
    if (!client.Start()) {
        return 1;
    }
    
    // Run simulation in a separate thread
    std::thread simulation_thread([&client]() {
        client.SimulateSensors(20);  // Run for 20 seconds
        client.Stop();
    });
    
    // Run MQTT loop
    client.Run();
    
    if (simulation_thread.joinable()) {
        simulation_thread.join();
    }
    
    std::cout << "✅ MQTT-Only approach test completed!" << std::endl;
    return 0;
} 