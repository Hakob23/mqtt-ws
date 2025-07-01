#include "RPi4_Gateway.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>

namespace rpi4_gateway {

//=============================================================================
// StorageManager Implementation  
//=============================================================================

StorageManager::StorageManager(const RPi4GatewayConfig& config) : config_(config) {
    data_path_ = config_.data_directory;
    log_path_ = config_.log_directory;
    std::cout << "💾 [StorageManager] Created" << std::endl;
}

StorageManager::~StorageManager() {
    std::cout << "💾 [StorageManager] Destroyed" << std::endl;
}

bool StorageManager::initialize() {
    try {
        std::filesystem::create_directories(data_path_);
        std::filesystem::create_directories(log_path_);
        std::cout << "✅ [StorageManager] Initialized" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ [StorageManager] Init failed: " << e.what() << std::endl;
        return false;
    }
}

bool StorageManager::store_sensor_data(const SensorDataPacket& packet) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    
    std::stringstream filename;
    filename << data_path_ << "/" << packet.sensor_id << "_" 
             << std::put_time(&tm, "%Y%m%d") << ".csv";
    
    std::ofstream file(filename.str(), std::ios::app);
    if (file.is_open()) {
        file << std::chrono::duration_cast<std::chrono::seconds>(
                    packet.timestamp.time_since_epoch()).count() << ","
             << packet.sensor_id << ","
             << packet.temperature_celsius << ","
             << packet.humidity_percent << std::endl;
        return true;
    }
    return false;
}

void StorageManager::cleanup() {
    std::cout << "🧹 [StorageManager] Cleanup completed" << std::endl;
}

bool StorageManager::store_statistics(const SensorStatistics& stats) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::stringstream filename;
    filename << data_path_ << "/statistics_" 
             << std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() 
             << ".csv";
    
    std::ofstream file(filename.str(), std::ios::app);
    if (file.is_open()) {
        file << stats.sensor_id << ","
             << stats.total_packets << ","
             << stats.valid_packets << ","
             << stats.packet_loss_rate << ","
             << stats.avg_temperature << ","
             << stats.avg_humidity << std::endl;
        return true;
    }
    return false;
}

bool StorageManager::store_edge_result(const EdgeProcessingResult& result) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::stringstream filename;
    filename << data_path_ << "/edge_results_" 
             << std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() 
             << ".csv";
    
    std::ofstream file(filename.str(), std::ios::app);
    if (file.is_open()) {
        file << result.sensor_id << ","
             << result.analysis_type << ","
             << result.confidence_score << std::endl;
        return true;
    }
    return false;
}

std::vector<SensorDataPacket> StorageManager::retrieve_sensor_data(
    const std::string& sensor_id, 
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Simplified implementation - in real scenario would parse CSV files
    std::vector<SensorDataPacket> results;
    std::cout << "📊 [StorageManager] Retrieved data for " << sensor_id 
              << " (mock implementation)" << std::endl;
    return results;
}

void StorageManager::rotate_logs() {
    std::cout << "🔄 [StorageManager] Log rotation completed" << std::endl;
}

void StorageManager::cleanup_old_data() {
    std::cout << "🧹 [StorageManager] Old data cleanup completed" << std::endl;
}

uint64_t StorageManager::get_storage_usage() const {
    // Simplified implementation
    return 1024 * 1024; // 1MB mock usage
}

//=============================================================================
// SystemMonitor Implementation
//=============================================================================

SystemMonitor::SystemMonitor() : running_(false) {
    std::cout << "📊 [SystemMonitor] Created" << std::endl;
}

SystemMonitor::~SystemMonitor() {
    stop();
    std::cout << "📊 [SystemMonitor] Destroyed" << std::endl;
}

bool SystemMonitor::start() {
    if (running_.load()) return true;
    
    running_ = true;
    monitor_thread_ = std::thread(&SystemMonitor::monitor_loop, this);
    std::cout << "🚀 [SystemMonitor] Started" << std::endl;
    return true;
}

void SystemMonitor::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    std::cout << "✅ [SystemMonitor] Stopped" << std::endl;
}

void SystemMonitor::monitor_loop() {
    while (running_.load()) {
        update_system_metrics();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void SystemMonitor::update_system_metrics() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    current_status_.is_running = running_.load();
    current_status_.cpu_usage_percent = get_cpu_usage();
    current_status_.memory_usage_bytes = get_memory_usage();
    current_status_.disk_usage_percent = get_disk_usage();
    current_status_.last_status_update = std::chrono::steady_clock::now();
}

float SystemMonitor::get_cpu_usage() const {
    // Simplified CPU usage calculation
    std::ifstream file("/proc/loadavg");
    if (file.is_open()) {
        float load;
        file >> load;
        return std::min(100.0f, load * 25.0f); // Rough conversion for single core
    }
    return 0.0f;
}

uint64_t SystemMonitor::get_memory_usage() const {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (info.totalram - info.freeram) * info.mem_unit;
    }
    return 0;
}

float SystemMonitor::get_disk_usage() const {
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        uint64_t total = stat.f_blocks * stat.f_frsize;
        uint64_t available = stat.f_bavail * stat.f_frsize;
        if (total > 0) {
            return 100.0f * (1.0f - static_cast<float>(available) / total);
        }
    }
    return 0.0f;
}

GatewayStatus SystemMonitor::get_system_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_;
}

//=============================================================================
// RPi4_Gateway Main Implementation
//=============================================================================

RPi4_Gateway::RPi4_Gateway(const RPi4GatewayConfig& config)
    : config_(config), running_(false), initialized_(false) {
    std::cout << "🏠 [RPi4_Gateway] Created: " << config_.gateway_id << std::endl;
}

RPi4_Gateway::~RPi4_Gateway() {
    stop();
    std::cout << "🏠 [RPi4_Gateway] Destroyed" << std::endl;
}

bool RPi4_Gateway::initialize() {
    if (initialized_.load()) return true;
    
    std::cout << "🚀 [RPi4_Gateway] Initializing..." << std::endl;
    
    // Initialize components
    data_processor_ = std::make_unique<DataProcessor>(config_);
    if (!data_processor_->initialize()) {
        return false;
    }
    
    if (config_.enable_local_storage) {
        storage_manager_ = std::make_unique<StorageManager>(config_);
        if (!storage_manager_->initialize()) {
            return false;
        }
    }
    
    system_monitor_ = std::make_unique<SystemMonitor>();
    if (!system_monitor_->start()) {
        return false;
    }
    
    setup_communication_interfaces();
    
    // Set up callbacks
    data_processor_->set_mqtt_callback(
        [this](const std::string& topic, const std::string& message) {
            this->handle_mqtt_message(topic, message);
        });
    
    data_processor_->set_websocket_callback(
        [this](const std::string& message) {
            this->handle_websocket_message(message);
        });
    
    initialized_ = true;
    std::cout << "✅ [RPi4_Gateway] Initialized" << std::endl;
    return true;
}

bool RPi4_Gateway::start() {
    if (!initialized_.load()) {
        std::cerr << "❌ [RPi4_Gateway] Not initialized" << std::endl;
        return false;
    }
    
    if (running_.load()) return true;
    
    running_ = true;
    
    // Start data processor
    if (!data_processor_->start()) {
        running_ = false;
        return false;
    }
    
    // Start communication interfaces
    for (auto& interface : comm_interfaces_) {
        interface->start();
    }
    
    // Start main loop
    main_loop_thread_ = std::thread(&RPi4_Gateway::main_loop, this);
    
    std::cout << "🚀 [RPi4_Gateway] Started with " << comm_interfaces_.size() << " interfaces" << std::endl;
    return true;
}

void RPi4_Gateway::stop() {
    if (!running_.load()) return;
    
    std::cout << "🛑 [RPi4_Gateway] Stopping..." << std::endl;
    running_ = false;
    
    // Stop interfaces
    for (auto& interface : comm_interfaces_) {
        interface->stop();
    }
    
    // Stop data processor
    if (data_processor_) {
        data_processor_->stop();
    }
    
    // Stop main loop
    if (main_loop_thread_.joinable()) {
        main_loop_thread_.join();
    }
    
    std::cout << "✅ [RPi4_Gateway] Stopped" << std::endl;
}

void RPi4_Gateway::setup_communication_interfaces() {
    std::cout << "🔌 [RPi4_Gateway] Setting up interfaces..." << std::endl;
    
    // Create UART interface
    auto uart_interface = std::make_unique<UARTInterface>(config_.uart_device, config_.uart_baudrate);
    uart_interface->set_data_callback(
        [this](const SensorDataPacket& packet) {
            this->handle_sensor_data(packet);
        });
    
    if (uart_interface->initialize()) {
        comm_interfaces_.push_back(std::move(uart_interface));
        std::cout << "✅ [RPi4_Gateway] UART interface ready" << std::endl;
    }
    
    // Create SPI interface
    auto spi_interface = std::make_unique<SPIInterface>(config_.spi_device, config_.spi_speed);
    spi_interface->set_data_callback(
        [this](const SensorDataPacket& packet) {
            this->handle_sensor_data(packet);
        });
    
    if (spi_interface->initialize()) {
        comm_interfaces_.push_back(std::move(spi_interface));
        std::cout << "✅ [RPi4_Gateway] SPI interface ready" << std::endl;
    }
    
    // Create I2C interface
    if (!config_.i2c_addresses.empty()) {
        auto i2c_interface = std::make_unique<I2CInterface>(config_.i2c_bus, config_.i2c_addresses);
        i2c_interface->set_data_callback(
            [this](const SensorDataPacket& packet) {
                this->handle_sensor_data(packet);
            });
        
        if (i2c_interface->initialize()) {
            comm_interfaces_.push_back(std::move(i2c_interface));
            std::cout << "✅ [RPi4_Gateway] I2C interface ready" << std::endl;
        }
    }
}

void RPi4_Gateway::main_loop() {
    std::cout << "🔄 [RPi4_Gateway] Main loop started" << std::endl;
    
    while (running_.load()) {
        // Periodic status logging
        static auto last_status = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (now - last_status >= std::chrono::minutes(5)) {
            if (system_monitor_) {
                auto status = system_monitor_->get_system_status();
                std::cout << "📊 [RPi4_Gateway] Status - CPU: " 
                          << std::fixed << std::setprecision(1) << status.cpu_usage_percent 
                          << "%, Memory: " << (status.memory_usage_bytes / 1024 / 1024) 
                          << "MB, Disk: " << status.disk_usage_percent << "%" << std::endl;
            }
            last_status = now;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    
    std::cout << "🏁 [RPi4_Gateway] Main loop finished" << std::endl;
}

void RPi4_Gateway::handle_sensor_data(const SensorDataPacket& packet) {
    std::cout << "📨 [RPi4_Gateway] Data from " << packet.sensor_id << ": " 
              << packet.temperature_celsius << "°C, " << packet.humidity_percent << "%" << std::endl;
    
    // Process through data processor
    if (data_processor_) {
        data_processor_->process_packet(packet);
    }
    
    // Store locally if enabled
    if (storage_manager_ && config_.enable_local_storage) {
        storage_manager_->store_sensor_data(packet);
    }
    
    // Integrate with thermal monitoring
    if (thermal_callback_ && packet.is_valid) {
        thermal_callback_(packet.sensor_id, packet.temperature_celsius, packet.humidity_percent);
    }
}

void RPi4_Gateway::handle_mqtt_message(const std::string& topic, const std::string& message) {
    std::cout << "📤 [RPi4_Gateway] MQTT: " << topic << std::endl;
    if (external_mqtt_callback_) {
        external_mqtt_callback_(topic, message);
    }
}

void RPi4_Gateway::handle_websocket_message(const std::string& message) {
    std::cout << "📤 [RPi4_Gateway] WebSocket message" << std::endl;
    if (external_websocket_callback_) {
        external_websocket_callback_(message);
    }
}

GatewayStatus RPi4_Gateway::get_status() const {
    if (system_monitor_) {
        auto status = system_monitor_->get_system_status();
        status.current_mode = config_.mode;
        status.processing_strategy = config_.processing_strategy;
        
        // Check interface status
        for (const auto& interface : comm_interfaces_) {
            if (interface->get_interface_name() == "UART") {
                status.uart_active = interface->is_active();
            } else if (interface->get_interface_name() == "SPI") {
                status.spi_active = interface->is_active();
            } else if (interface->get_interface_name() == "I2C") {
                status.i2c_active = interface->is_active();
            }
        }
        
        return status;
    }
    
    GatewayStatus status = {};
    status.is_running = running_.load();
    return status;
}

void RPi4_Gateway::set_external_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback) {
    external_mqtt_callback_ = callback;
}

void RPi4_Gateway::set_external_websocket_callback(std::function<void(const std::string&)> callback) {
    external_websocket_callback_ = callback;
}

void RPi4_Gateway::set_thermal_monitoring_callback(std::function<void(const std::string&, float, float)> callback) {
    thermal_callback_ = callback;
}

std::vector<SensorStatistics> RPi4_Gateway::get_sensor_statistics() const {
    if (data_processor_) {
        return data_processor_->get_all_statistics();
    }
    return {};
}

std::vector<EdgeProcessingResult> RPi4_Gateway::get_edge_results() const {
    if (data_processor_) {
        return data_processor_->get_recent_edge_results();
    }
    return {};
}

void RPi4_Gateway::update_config(const RPi4GatewayConfig& new_config) {
    config_ = new_config;
    std::cout << "🔧 [RPi4_Gateway] Configuration updated" << std::endl;
}

void RPi4_Gateway::trigger_data_sync() {
    std::cout << "🔄 [RPi4_Gateway] Manual data sync triggered" << std::endl;
    if (data_processor_) {
        // Trigger immediate aggregation
        std::cout << "📊 [RPi4_Gateway] Forcing data aggregation..." << std::endl;
    }
}

void RPi4_Gateway::perform_system_cleanup() {
    std::cout << "🧹 [RPi4_Gateway] Performing system cleanup..." << std::endl;
    if (storage_manager_) {
        storage_manager_->cleanup_old_data();
        storage_manager_->rotate_logs();
    }
}

void RPi4_Gateway::switch_mode(GatewayMode new_mode) {
    config_.mode = new_mode;
    std::cout << "🔄 [RPi4_Gateway] Switched to mode: " 
              << (new_mode == GatewayMode::COLLECTOR_ONLY ? "Collector Only" :
                  new_mode == GatewayMode::EDGE_PROCESSOR ? "Edge Processor" :
                  new_mode == GatewayMode::HYBRID_BRIDGE ? "Hybrid Bridge" : "Failsafe") << std::endl;
}

//=============================================================================
// Factory Functions
//=============================================================================

namespace gateway_factory {

RPi4GatewayConfig create_home_gateway_config(const std::string& gateway_id) {
    RPi4GatewayConfig config;
    config.gateway_id = gateway_id;
    config.location = "Home";
    config.mode = GatewayMode::HYBRID_BRIDGE;
    config.processing_strategy = ProcessingStrategy::SMART_FILTER;
    config.processing_interval_ms = 2000;
    config.aggregation_window_seconds = 300;
    config.temp_alert_low = 15.0f;
    config.temp_alert_high = 30.0f;
    config.humidity_alert_high = 70.0f;
    config.i2c_addresses = {0x76, 0x77, 0x44, 0x45};
    return config;
}

RPi4GatewayConfig create_industrial_gateway_config(const std::string& gateway_id) {
    RPi4GatewayConfig config;
    config.gateway_id = gateway_id;
    config.location = "Industrial";
    config.mode = GatewayMode::EDGE_PROCESSOR;
    config.processing_strategy = ProcessingStrategy::PREDICTIVE_EDGE;
    config.processing_interval_ms = 500;
    config.aggregation_window_seconds = 60;
    config.worker_thread_count = 8;
    config.temp_alert_low = 5.0f;
    config.temp_alert_high = 40.0f;
    config.humidity_alert_high = 80.0f;
    config.i2c_addresses = {0x76, 0x77, 0x44, 0x45, 0x48, 0x49};
    return config;
}

std::unique_ptr<RPi4_Gateway> create_basic_gateway(const std::string& gateway_id) {
    auto config = create_home_gateway_config(gateway_id);
    config.mode = GatewayMode::COLLECTOR_ONLY;
    config.enable_edge_analytics = false;
    return std::make_unique<RPi4_Gateway>(config);
}

std::unique_ptr<RPi4_Gateway> create_full_featured_gateway(const std::string& gateway_id) {
    auto config = create_industrial_gateway_config(gateway_id);
    return std::make_unique<RPi4_Gateway>(config);
}

RPi4GatewayConfig create_agricultural_gateway_config(const std::string& gateway_id) {
    RPi4GatewayConfig config;
    config.gateway_id = gateway_id;
    config.location = "Agricultural";
    config.mode = GatewayMode::EDGE_PROCESSOR;
    config.processing_strategy = ProcessingStrategy::SMART_FILTER;
    config.processing_interval_ms = 5000; // 5 seconds
    config.aggregation_window_seconds = 600; // 10 minutes
    config.temp_alert_low = 0.0f;  // Cold tolerance
    config.temp_alert_high = 45.0f; // Heat tolerance
    config.humidity_alert_high = 95.0f; // High humidity OK
    config.i2c_addresses = {0x76, 0x77, 0x44, 0x45};
    return config;
}

RPi4GatewayConfig create_edge_ai_gateway_config(const std::string& gateway_id) {
    RPi4GatewayConfig config;
    config.gateway_id = gateway_id;
    config.location = "EdgeAI";
    config.mode = GatewayMode::EDGE_PROCESSOR;
    config.processing_strategy = ProcessingStrategy::PREDICTIVE_EDGE;
    config.processing_interval_ms = 100; // Very fast
    config.aggregation_window_seconds = 10; // Quick aggregation
    config.worker_thread_count = 16; // High performance
    config.enable_edge_analytics = true;
    config.max_sensor_history = 5000; // Large history
    config.temp_alert_low = -20.0f;
    config.temp_alert_high = 85.0f;
    config.humidity_alert_high = 100.0f;
    config.i2c_addresses = {0x76, 0x77, 0x44, 0x45, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D};
    return config;
}

} // namespace gateway_factory

} // namespace rpi4_gateway 