#include "../include/mqtt_ws_bridge.h"
#include <iostream>
#include <signal.h>
#include <fstream>
#include <jsoncpp/json/json.h>

using namespace mqtt_ws;

// Global bridge instance for signal handling
static std::unique_ptr<MqttWebSocketBridge> g_bridge;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    if (g_bridge) {
        g_bridge->stop();
    }
    exit(0);
}

BridgeConfig load_config(const std::string& config_file) {
    BridgeConfig config;
    
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file " << config_file << ", using defaults" << std::endl;
        return config;
    }
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(file, root)) {
        std::cerr << "Warning: Could not parse config file, using defaults" << std::endl;
        return config;
    }
    
    // Parse MQTT settings
    if (root.isMember("mqtt")) {
        auto mqtt = root["mqtt"];
        if (mqtt.isMember("host")) config.mqtt_host = mqtt["host"].asString();
        if (mqtt.isMember("port")) config.mqtt_port = mqtt["port"].asInt();
    }
    
    // Parse WebSocket settings
    if (root.isMember("websocket")) {
        auto ws = root["websocket"];
        if (ws.isMember("port")) config.websocket_port = ws["port"].asInt();
        if (ws.isMember("host")) config.websocket_host = ws["host"].asString();
        if (ws.isMember("ssl_cert")) config.ssl_cert_path = ws["ssl_cert"].asString();
        if (ws.isMember("ssl_key")) config.ssl_key_path = ws["ssl_key"].asString();
    }
    
    return config;
}

void print_usage(const char* program_name) {
    std::cout << "MQTT-WebSocket Bridge (C++)" << std::endl;
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --config FILE    Configuration file path" << std::endl;
    std::cout << "  -h, --host HOST      MQTT broker host (default: localhost)" << std::endl;
    std::cout << "  -p, --port PORT      MQTT broker port (default: 1883)" << std::endl;
    std::cout << "  -l, --listen PORT    WebSocket listen port (default: 8080)" << std::endl;
    std::cout << "  -t, --threads NUM    Number of worker threads (default: auto)" << std::endl;

    std::cout << "  --help               Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== MQTT-WebSocket Bridge (C++) ===" << std::endl;
    std::cout << "Optimized for Linux with zero-copy message handling" << std::endl;
    std::cout << "================================================================" << std::endl;
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    BridgeConfig config;
    std::string config_file;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) config.mqtt_host = argv[++i];
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) config.mqtt_port = std::atoi(argv[++i]);
        } else if (arg == "-l" || arg == "--listen") {
            if (i + 1 < argc) config.websocket_port = std::atoi(argv[++i]);
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) config_file = argv[++i];
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) config.worker_threads = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    // Load configuration file if specified
    if (!config_file.empty()) {
        config = load_config(config_file);
    }
    
    try {
        // Create and initialize bridge
        g_bridge = std::make_unique<MqttWebSocketBridge>(config);
        
        if (!g_bridge->initialize()) {
            std::cerr << "Failed to initialize bridge" << std::endl;
            return 1;
        }
        
        if (!g_bridge->start()) {
            std::cerr << "Failed to start bridge" << std::endl;
            return 1;
        }
        
        std::cout << "\n🚀 Bridge is running! Press Ctrl+C to stop." << std::endl;
        
        // Main loop - wait for signal
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
