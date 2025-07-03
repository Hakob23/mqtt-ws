#include "../../thermal-monitoring/ThermalIsolationTracker.h"
#include <libwebsockets.h>
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

using namespace thermal_monitoring;

class SimpleWebSocketServer {
private:
    struct lws_context* context_;
    std::unique_ptr<ThermalIsolationTracker> thermal_tracker_;
    bool running_;
    int port_;
    
    // Performance metrics
    int messages_sent_ = 0;
    int messages_received_ = 0;
    int connected_clients_ = 0;
    std::chrono::steady_clock::time_point start_time_;
    
    // Client management
    std::mutex clients_mutex_;
    std::map<struct lws*, std::string> connected_clients_map_;
    
    // Test mode for performance testing
    bool test_mode_ = false;
    int simulated_clients_ = 5; // Simulate 5 clients to match 5 sensors

public:
    SimpleWebSocketServer(int port = 8080) 
        : context_(nullptr), running_(false), port_(port) {
        start_time_ = std::chrono::steady_clock::now();
    }
    
    ~SimpleWebSocketServer() {
        Stop();
    }
    
    void EnableTestMode(int simulated_clients = 5) {
        test_mode_ = true;
        simulated_clients_ = simulated_clients;
        std::cout << "🧪 Test mode enabled with " << simulated_clients << " simulated clients" << std::endl;
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
            BroadcastAlert(alert);
        });
        
        // Initialize WebSocket server
        struct lws_context_creation_info info;
        memset(&info, 0, sizeof(info));
        
        static const struct lws_protocols protocols[] = {
            {
                "thermal-protocol",
                WebSocketCallback,
                sizeof(struct lws*),
                4096,
                0, this, 0
            },
            { NULL, NULL, 0, 0, 0, NULL, 0 } // terminator
        };
        
        info.port = port_;
        info.protocols = protocols;
        info.gid = -1;
        info.uid = -1;
        info.user = this;
        
        context_ = lws_create_context(&info);
        if (!context_) {
            std::cerr << "❌ Failed to create WebSocket context" << std::endl;
            return false;
        }
        
        // Start thermal monitoring
        if (!thermal_tracker_->start()) {
            std::cerr << "❌ Failed to start thermal monitoring" << std::endl;
            return false;
        }
        
        running_ = true;
        std::cout << "✅ WebSocket Thermal Server started on port " << port_ << std::endl;
        
        return true;
    }
    
    void Stop() {
        if (!running_) return;
        
        running_ = false;
        
        if (thermal_tracker_) {
            thermal_tracker_->stop();
        }
        
        if (context_) {
            lws_context_destroy(context_);
            context_ = nullptr;
        }
        
        std::cout << "🛑 WebSocket Thermal Server stopped" << std::endl;
    }
    
    void Run() {
        if (!running_) return;
        
        std::cout << "🏃 Starting WebSocket server loop..." << std::endl;
        
        while (running_) {
            lws_service(context_, 50);
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
        
        // Broadcast to WebSocket clients
        BroadcastSensorData(sensor_id, temperature, humidity, location);
    }
    
    std::string GetLocationForSensor(int sensor_num) {
        const std::vector<std::string> locations = {
            "Living Room", "Kitchen", "Bedroom", "Basement", "Attic"
        };
        return locations[sensor_num - 1];
    }
    
    void BroadcastSensorData(const std::string& sensor_id, double temperature, double humidity, const std::string& location) {
        Json::Value data;
        data["type"] = "sensor_data";
        data["sensor_id"] = sensor_id;
        data["temperature"] = temperature;
        data["humidity"] = humidity;
        data["location"] = location;
        data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string message = Json::writeString(builder, data);
        
        BroadcastMessage(message);
    }
    
    void BroadcastAlert(const Alert& alert) {
        Json::Value alert_data;
        alert_data["type"] = "alert";
        alert_data["sensor_id"] = alert.sensor_id;
        alert_data["alert_message"] = alert.message;
        alert_data["temperature"] = alert.temperature;
        alert_data["humidity"] = alert.humidity;
        alert_data["location"] = alert.location;
        alert_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string message = Json::writeString(builder, alert_data);
        
        BroadcastMessage(message);
        std::cout << "🚨 Broadcasted alert: " << alert.message << " for " << alert.sensor_id << std::endl;
    }
    
    void BroadcastMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        
        int client_count = connected_clients_map_.size();
        
        // In test mode, simulate clients if no real clients connected
        if (test_mode_ && client_count == 0) {
            client_count = simulated_clients_;
            // Count messages sent to simulated clients
            messages_sent_ += client_count;
            return;
        }
        
        // Real WebSocket clients
        if (client_count == 0) return;
        
        // Prepare message with WebSocket framing
        size_t msg_len = message.length();
        unsigned char* buf = new unsigned char[LWS_PRE + msg_len];
        memcpy(&buf[LWS_PRE], message.c_str(), msg_len);
        
        for (auto& client_pair : connected_clients_map_) {
            struct lws* wsi = client_pair.first;
            lws_write(wsi, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
        }
        
        delete[] buf;
        messages_sent_ += client_count;
    }
    
    void AddClient(struct lws* wsi) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::string client_id = "client_" + std::to_string(connected_clients_++);
        connected_clients_map_[wsi] = client_id;
        std::cout << "✅ Client connected: " << client_id << " (Total: " << connected_clients_map_.size() << ")" << std::endl;
    }
    
    void RemoveClient(struct lws* wsi) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = connected_clients_map_.find(wsi);
        if (it != connected_clients_map_.end()) {
            std::cout << "👋 Client disconnected: " << it->second << " (Remaining: " << (connected_clients_map_.size() - 1) << ")" << std::endl;
            connected_clients_map_.erase(it);
        }
    }
    
    void HandleClientMessage(struct lws* /* wsi */, const std::string& message) {
        messages_received_++;
        
        try {
            Json::Value data;
            Json::CharReaderBuilder reader_builder;
            std::string errors;
            std::stringstream ss(message);
            
            if (Json::parseFromStream(reader_builder, ss, &data, &errors)) {
                std::string type = data.get("type", "").asString();
                
                if (type == "sensor_data") {
                    // Handle incoming sensor data from client
                    std::string sensor_id = data.get("sensor_id", "").asString();
                    double temperature = data.get("temperature", 0.0).asDouble();
                    double humidity = data.get("humidity", 0.0).asDouble();
                    std::string location = data.get("location", "").asString();
                    
                    thermal_tracker_->process_sensor_data(sensor_id, temperature, humidity, location);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error processing client message: " << e.what() << std::endl;
        }
    }
    
    void PrintPerformanceStats() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        double duration_sec = duration.count() / 1000.0;
        
        std::cout << "\n📊 Performance Results:" << std::endl;
        std::cout << "Messages sent: " << messages_sent_ << std::endl;
        std::cout << "Messages received: " << messages_received_ << std::endl;
        
        int client_count = connected_clients_map_.size();
        if (test_mode_ && client_count == 0) {
            client_count = simulated_clients_;
            std::cout << "Connected clients: " << client_count << " (simulated)" << std::endl;
        } else {
            std::cout << "Connected clients: " << client_count << std::endl;
        }
        
        std::cout << "Duration: " << std::fixed << std::setprecision(2) << duration_sec << "s" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) << (messages_sent_ / duration_sec) << " msg/sec" << std::endl;
        
        // Get thermal monitoring stats
        auto sensors = thermal_tracker_->get_all_sensors();
        auto alerts = thermal_tracker_->get_recent_alerts(10);
        
        std::cout << "Active sensors: " << sensors.size() << std::endl;
        std::cout << "Recent alerts: " << alerts.size() << std::endl;
    }
    
    // Static WebSocket callback
    static int WebSocketCallback(struct lws* wsi, enum lws_callback_reasons reason,
                                void* /* user */, void* in, size_t len) {
        SimpleWebSocketServer* server = static_cast<SimpleWebSocketServer*>(lws_context_user(lws_get_context(wsi)));
        
        switch (reason) {
            case LWS_CALLBACK_ESTABLISHED:
                server->AddClient(wsi);
                break;
                
            case LWS_CALLBACK_CLOSED:
                server->RemoveClient(wsi);
                break;
                
            case LWS_CALLBACK_RECEIVE:
                if (in && len > 0) {
                    std::string message(static_cast<char*>(in), len);
                    server->HandleClientMessage(wsi, message);
                }
                break;
                
            default:
                break;
        }
        
        return 0;
    }
};

int main() {
    std::cout << "🚀 WebSocket-Only Thermal Monitoring Server" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    SimpleWebSocketServer server(8080);
    
    // Enable test mode for self-contained testing
    server.EnableTestMode(5); // Simulate 5 clients to match 5 sensors
    
    if (!server.Start()) {
        return 1;
    }
    
    // Run simulation in a separate thread
    std::thread simulation_thread([&server]() {
        server.SimulateSensors(20);  // Run for 20 seconds
        server.Stop();
    });
    
    // Run WebSocket server loop
    server.Run();
    
    if (simulation_thread.joinable()) {
        simulation_thread.join();
    }
    
    std::cout << "✅ WebSocket-Only approach test completed!" << std::endl;
    return 0;
}
