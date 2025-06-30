#include "STM32_SensorNode.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>
#include <iomanip>

using namespace stm32_simulation;

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

// Signal handler for Ctrl+C
void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running = false;
}

// UART callback - simulates data received from STM32 via UART
void uart_data_handler(const std::string& node_id, const std::vector<uint8_t>& data) {
    std::cout << "📨 UART [" << node_id << "] Received " << data.size() << " bytes: ";
    
    // Print first few bytes in hex
    for (size_t i = 0; i < std::min(data.size(), size_t(8)); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > 8) {
        std::cout << "...";
    }
    std::cout << std::dec << std::endl;
    
    // Decode the binary packet (simplified)
    if (data.size() >= 14 && data[0] == 0xAA && data[1] == 0xBB) {
        // Extract temperature and humidity
        int16_t temp_raw = (data[6] << 8) | data[7];
        uint16_t hum_raw = (data[8] << 8) | data[9];
        uint16_t volt_raw = (data[10] << 8) | data[11];
        uint8_t status = data[12];
        
        float temperature = temp_raw / 100.0f;
        float humidity = hum_raw / 100.0f;
        float voltage = volt_raw / 1000.0f;
        
        std::cout << "   📊 Decoded: T=" << temperature << "°C, H=" << humidity 
                  << "%, V=" << voltage << "V, Status=0x" << std::hex << static_cast<int>(status) 
                  << std::dec << std::endl;
    }
}

// MQTT callback - simulates MQTT message publishing
void mqtt_message_handler(const std::string& topic, const std::string& message) {
    std::cout << "📤 MQTT Topic: " << topic << std::endl;
    std::cout << "   Message: " << message << std::endl;
}

// Test individual sensor node
void test_single_sensor() {
    std::cout << "\n🧪 Testing Individual STM32 Sensor Node\n";
    std::cout << "=========================================\n";
    
    // Create a DHT22 sensor node
    auto config = sensor_factory::create_indoor_node("test_sensor", "Test Location");
    config.reading_interval_ms = 1000;
    config.transmission_interval_ms = 2000;
    
    STM32_SensorNode sensor(config);
    
    // Set up callbacks
    sensor.set_uart_callback(uart_data_handler);
    sensor.set_mqtt_callback(mqtt_message_handler);
    
    // Initialize and start
    if (!sensor.initialize() || !sensor.start()) {
        std::cerr << "❌ Failed to start sensor node" << std::endl;
        return;
    }
    
    // Run for 10 seconds
    std::cout << "🏃 Running single sensor test for 10 seconds...\n";
    auto start_time = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(10)) {
        std::cout << "Status: " << sensor.get_status() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    // Test fault injection
    std::cout << "\n🚨 Testing fault injection...\n";
    sensor.inject_fault();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Test power loss simulation
    std::cout << "\n⚡ Testing power loss simulation...\n";
    sensor.simulate_power_loss(2000);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    sensor.stop();
    std::cout << "✅ Single sensor test completed\n";
}

// Test sensor deployment
void test_sensor_deployment() {
    std::cout << "\n🏭 Testing Sensor Deployment\n";
    std::cout << "=============================\n";
    
    // Create a home deployment
    auto deployment = sensor_factory::create_home_deployment();
    
    // Set up global callbacks
    deployment->set_global_uart_callback(uart_data_handler);
    deployment->set_global_mqtt_callback(mqtt_message_handler);
    
    std::cout << "Deployment created with " << deployment->get_node_count() << " nodes\n";
    
    // Print node IDs
    auto node_ids = deployment->get_node_ids();
    std::cout << "Nodes: ";
    for (const auto& id : node_ids) {
        std::cout << id << " ";
    }
    std::cout << std::endl;
    
    // Start all nodes
    if (!deployment->start_all()) {
        std::cerr << "❌ Failed to start deployment" << std::endl;
        return;
    }
    
    // Run deployment for 15 seconds
    std::cout << "\n🏃 Running deployment test for 15 seconds...\n";
    auto start_time = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(15)) {
        std::cout << "\n" << deployment->get_deployment_status() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    // Test bulk operations
    std::cout << "\n🔄 Testing bulk operations...\n";
    
    // Inject random faults
    std::cout << "Injecting random faults (20% chance)...\n";
    deployment->inject_random_faults(0.2f);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Simulate power outage
    std::cout << "Simulating power outage for 2 seconds...\n";
    deployment->simulate_power_outage(2000);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    
    // Change environment
    std::cout << "Changing all environments to heating cycle...\n";
    deployment->change_all_environments(EnvironmentPattern::HEATING_CYCLE);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Collect and display readings
    std::cout << "\n📊 Collecting final readings from all sensors...\n";
    auto readings = deployment->collect_all_readings();
    for (const auto& reading : readings) {
        if (reading.is_valid) {
            std::cout << "  T: " << std::fixed << std::setprecision(1) << reading.temperature_celsius 
                      << "°C, H: " << reading.humidity_percent << "%" << std::endl;
        }
    }
    
    // Save deployment log
    deployment->save_deployment_log("deployment_test_log.txt");
    
    deployment->stop_all();
    std::cout << "✅ Deployment test completed\n";
}

// Test different sensor types
void test_different_sensor_types() {
    std::cout << "\n🔬 Testing Different Sensor Types\n";
    std::cout << "==================================\n";
    
    std::vector<std::unique_ptr<STM32_SensorNode>> sensors;
    
    // Create different sensor types
    auto dht22_config = sensor_factory::create_indoor_node("dht22", "DHT22 Test");
    dht22_config.sensor_type = SensorType::DHT22;
    sensors.push_back(std::make_unique<STM32_SensorNode>(dht22_config));
    
    auto bme280_config = sensor_factory::create_outdoor_node("bme280", "BME280 Test");
    bme280_config.sensor_type = SensorType::BME280;
    sensors.push_back(std::make_unique<STM32_SensorNode>(bme280_config));
    
    auto sht30_config = sensor_factory::create_industrial_node("sht30", "SHT30 Test");
    sht30_config.sensor_type = SensorType::SHT30;
    sensors.push_back(std::make_unique<STM32_SensorNode>(sht30_config));
    
    auto faulty_config = sensor_factory::create_indoor_node("faulty", "Faulty Test");
    faulty_config.sensor_type = SensorType::FAULTY_SENSOR;
    sensors.push_back(std::make_unique<STM32_SensorNode>(faulty_config));
    
    auto intermittent_config = sensor_factory::create_indoor_node("intermittent", "Intermittent Test");
    intermittent_config.sensor_type = SensorType::INTERMITTENT;
    sensors.push_back(std::make_unique<STM32_SensorNode>(intermittent_config));
    
    // Initialize and start all sensors
    for (auto& sensor : sensors) {
        sensor->set_uart_callback(uart_data_handler);
        sensor->set_mqtt_callback(mqtt_message_handler);
        
        if (!sensor->initialize() || !sensor->start()) {
            std::cerr << "❌ Failed to start sensor: " << sensor->get_node_id() << std::endl;
        }
    }
    
    // Run for 12 seconds
    std::cout << "\n🏃 Running sensor type comparison for 12 seconds...\n";
    auto start_time = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(12)) {
        std::cout << "\n--- Sensor Status ---\n";
        for (const auto& sensor : sensors) {
            std::cout << sensor->get_status() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    
    // Stop all sensors
    for (auto& sensor : sensors) {
        sensor->stop();
    }
    
    std::cout << "✅ Sensor type test completed\n";
}

// Test communication protocols
void test_communication_protocols() {
    std::cout << "\n📡 Testing Communication Protocols\n";
    std::cout << "===================================\n";
    
    std::vector<std::unique_ptr<STM32_SensorNode>> sensors;
    
    // Create sensors with different communication protocols
    auto uart_config = sensor_factory::create_indoor_node("uart_sensor", "UART Sensor");
    uart_config.comm_protocol = CommProtocol::UART_TO_GATEWAY;
    sensors.push_back(std::make_unique<STM32_SensorNode>(uart_config));
    
    auto mqtt_config = sensor_factory::create_indoor_node("mqtt_sensor", "MQTT Sensor");
    mqtt_config.comm_protocol = CommProtocol::MQTT_DIRECT;
    sensors.push_back(std::make_unique<STM32_SensorNode>(mqtt_config));
    
    auto spi_config = sensor_factory::create_indoor_node("spi_sensor", "SPI Sensor");
    spi_config.comm_protocol = CommProtocol::SPI_TO_GATEWAY;
    sensors.push_back(std::make_unique<STM32_SensorNode>(spi_config));
    
    auto i2c_config = sensor_factory::create_indoor_node("i2c_sensor", "I2C Sensor");
    i2c_config.comm_protocol = CommProtocol::I2C_TO_GATEWAY;
    sensors.push_back(std::make_unique<STM32_SensorNode>(i2c_config));
    
    // Set transmission intervals for faster demo
    for (auto& sensor : sensors) {
        sensor->set_uart_callback(uart_data_handler);
        sensor->set_mqtt_callback(mqtt_message_handler);
        
        if (!sensor->initialize() || !sensor->start()) {
            std::cerr << "❌ Failed to start sensor: " << sensor->get_node_id() << std::endl;
        }
    }
    
    std::cout << "\n🏃 Testing communication protocols for 10 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Stop all sensors
    for (auto& sensor : sensors) {
        sensor->stop();
    }
    
    std::cout << "✅ Communication protocol test completed\n";
}

// Interactive demo
void interactive_demo() {
    if (!g_running.load()) return;
    
    std::cout << "\n🎮 Interactive STM32 Sensor Demo\n";
    std::cout << "=================================\n";
    std::cout << "Creating a small office deployment...\n";
    
    auto deployment = sensor_factory::create_office_deployment();
    deployment->set_global_uart_callback(uart_data_handler);
    deployment->set_global_mqtt_callback(mqtt_message_handler);
    
    if (!deployment->start_all()) {
        std::cerr << "❌ Failed to start deployment" << std::endl;
        return;
    }
    
    std::cout << "\n🏃 Demo running... Press Ctrl+C to stop\n";
    std::cout << "Commands will be executed automatically every 30 seconds:\n";
    std::cout << "- Status check every 10 seconds\n";
    std::cout << "- Environment changes\n";
    std::cout << "- Fault injection\n";
    std::cout << "- Power simulation\n\n";
    
    int cycle = 0;
    auto last_status = std::chrono::steady_clock::now();
    auto last_action = std::chrono::steady_clock::now();
    
    while (g_running.load()) {
        auto now = std::chrono::steady_clock::now();
        
        // Status update every 10 seconds
        if (now - last_status >= std::chrono::seconds(10)) {
            std::cout << "\n📊 Status Update (Cycle " << cycle << "):\n";
            std::cout << deployment->get_deployment_status() << std::endl;
            last_status = now;
        }
        
        // Action every 30 seconds
        if (now - last_action >= std::chrono::seconds(30)) {
            switch (cycle % 4) {
                case 0:
                    std::cout << "\n🌡️ Changing environment to heating cycle...\n";
                    deployment->change_all_environments(EnvironmentPattern::HEATING_CYCLE);
                    break;
                case 1:
                    std::cout << "\n🚨 Injecting random faults (10% chance)...\n";
                    deployment->inject_random_faults(0.1f);
                    break;
                case 2:
                    std::cout << "\n❄️ Changing environment to cooling cycle...\n";
                    deployment->change_all_environments(EnvironmentPattern::COOLING_CYCLE);
                    break;
                case 3:
                    std::cout << "\n⚡ Simulating brief power outage (1 second)...\n";
                    deployment->simulate_power_outage(1000);
                    break;
            }
            last_action = now;
            cycle++;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    std::cout << "\n🛑 Stopping interactive demo...\n";
    deployment->stop_all();
    deployment->save_deployment_log("interactive_demo_log.txt");
    std::cout << "✅ Interactive demo completed\n";
}

int main() {
    // Set up signal handler
    signal(SIGINT, signal_handler);
    
    std::cout << "🚀 STM32 Sensor Node Simulator Test Suite\n";
    std::cout << "==========================================\n";
    
    try {
        // Test 1: Single sensor node
        test_single_sensor();
        
        if (!g_running.load()) return 0;
        
        // Test 2: Sensor deployment
        test_sensor_deployment();
        
        if (!g_running.load()) return 0;
        
        // Test 3: Different sensor types
        test_different_sensor_types();
        
        if (!g_running.load()) return 0;
        
        // Test 4: Communication protocols
        test_communication_protocols();
        
        if (!g_running.load()) return 0;
        
        // Interactive demo
        interactive_demo();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception caught: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n🎉 All STM32 simulator tests completed successfully!\n";
    std::cout << "Check the generated log files for detailed information.\n";
    
    return 0;
} 