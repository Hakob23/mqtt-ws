#include "RPi4_Gateway.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <fstream>

// System monitoring headers
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace rpi4_gateway {

//=============================================================================
// StorageManager Implementation
//=============================================================================

StorageManager::StorageManager(const RPi4GatewayConfig& config)
    : config_(config) {
    data_path_ = config_.data_directory;
    log_path_ = config_.log_directory;
    std::cout << "💾 [StorageManager] Created with data path: " << data_path_ << std::endl;
}

StorageManager::~StorageManager() {
    cleanup();
    std::cout << "💾 [StorageManager] Destroyed" << std::endl;
}

bool StorageManager::initialize() {
    std::cout << "🚀 [StorageManager] Initializing..." << std::endl;
    
    if (!ensure_directories()) {
        std::cerr << "❌ [StorageManager] Failed to create directories" << std::endl;
        return false;
    }
    
    // Check available space
    uint64_t available_mb = 0;
    struct statvfs stat;
    if (statvfs(data_path_.c_str(), &stat) == 0) {
        available_mb = (stat.f_bavail * stat.f_frsize) / (1024 * 1024);
        std::cout << "💾 [StorageManager] Available space: " << available_mb << " MB" << std::endl;
        
        if (available_mb < config_.max_storage_mb) {
            std::cout << "⚠️ [StorageManager] Low disk space warning" << std::endl;
        }
    }
    
    std::cout << "✅ [StorageManager] Initialized successfully" << std::endl;
    return true;
}

bool StorageManager::ensure_directories() {
    try {
        std::filesystem::create_directories(data_path_);
        std::filesystem::create_directories(log_path_);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ [StorageManager] Failed to create directories: " << e.what() << std::endl;
        return false;
    }
}

bool StorageManager::store_sensor_data(const SensorDataPacket& packet) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Create filename based on sensor ID and date
    auto now = std::chrono::system_clock::now();
    std::string filename = get_data_filename(packet.sensor_id, now);
    
    try {
        std::ofstream file(filename, std::ios::app);
        if (!file.is_open()) {
            return false;
        }
        
        // Write CSV format
        file << std::chrono::duration_cast<std::chrono::seconds>(
                    packet.timestamp.time_since_epoch()).count() << ","
             << packet.sensor_id << ","
             << packet.location << ","
             << std::fixed << std::setprecision(2) << packet.temperature_celsius << ","
             << packet.humidity_percent << ","
             << packet.pressure_hpa << ","
             << packet.supply_voltage << ","
             << static_cast<int>(packet.sensor_status) << ","
             << (packet.interface_used == CommInterface::UART_INTERFACE ? "UART" :
                 packet.interface_used == CommInterface::SPI_INTERFACE ? "SPI" :
                 packet.interface_used == CommInterface::I2C_INTERFACE ? "I2C" : "UNKNOWN") << ","
             << packet.signal_strength << ","
             << packet.data_confidence << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ [StorageManager] Failed to store data: " << e.what() << std::endl;
        return false;
    }
}

std::string StorageManager::get_data_filename(const std::string& sensor_id, 
                                            const std::chrono::system_clock::time_point& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto tm = *std::localtime(&time_t);
    
    std::stringstream ss;
    ss << data_path_ << "/" << sensor_id << "_" 
       << std::put_time(&tm, "%Y%m%d") << ".csv";
    
    return ss.str();
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
    if (running_.load()) {
        std::cout << "⚠️ [SystemMonitor] Already running" << std::endl;
        return true;
    }
    
    running_ = true;
    monitor_thread_ = std::thread(&SystemMonitor::monitor_loop, this);
    
    std::cout << "🚀 [SystemMonitor] Started" << std::endl;
    return true;
}

void SystemMonitor::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "🛑 [SystemMonitor] Stopping..." << std::endl;
    running_ = false;
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    std::cout << "✅ [SystemMonitor] Stopped" << std::endl;
}

void SystemMonitor::monitor_loop() {
    std::cout << "🔄 [SystemMonitor] Monitor thread started" << std::endl;
    
    while (running_.load()) {
        update_system_metrics();
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Update every 5 seconds
    }
    
    std::cout << "🏁 [SystemMonitor] Monitor thread finished" << std::endl;
}

void SystemMonitor::update_system_metrics() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    current_status_.is_running = running_.load();
    current_status_.cpu_usage_percent = get_cpu_usage();
    current_status_.memory_usage_bytes = get_memory_usage();
    current_status_.disk_usage_percent = get_disk_usage();
    current_status_.internet_connectivity = check_internet_connectivity();
    current_status_.last_status_update = std::chrono::steady_clock::now();
    
    // Network info
    std::string network_info = get_network_info();
    if (!network_info.empty()) {
        current_status_.network_interface = "eth0"; // Simplified
        current_status_.ip_address = network_info;
    }
}

float SystemMonitor::get_cpu_usage() const {
    static unsigned long long last_idle = 0, last_total = 0;
    
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return 0.0f;
    }
    
    std::string line;
    std::getline(file, line);
    
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (sscanf(line.c_str(), "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
        
        unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
        unsigned long long diff_idle = idle - last_idle;
        unsigned long long diff_total = total - last_total;
        
        float cpu_usage = 0.0f;
        if (diff_total > 0) {
            cpu_usage = 100.0f * (1.0f - static_cast<float>(diff_idle) / diff_total);
        }
        
        last_idle = idle;
        last_total = total;
        
        return cpu_usage;
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
        uint64_t used = total - available;
        
        if (total > 0) {
            return 100.0f * static_cast<float>(used) / total;
        }
    }
    return 0.0f;
}

std::string SystemMonitor::get_network_info() const {
    struct ifaddrs *ifaddrs_ptr;
    if (getifaddrs(&ifaddrs_ptr) == -1) {
        return "";
    }
    
    std::string ip_address;
    for (struct ifaddrs *ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        
        if (ifa->ifa_addr->sa_family == AF_INET && 
            strcmp(ifa->ifa_name, "lo") != 0) { // Skip loopback
            
            struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
            ip_address = inet_ntoa(addr_in->sin_addr);
            break;
        }
    }
    
    freeifaddrs(ifaddrs_ptr);
    return ip_address;
}

bool SystemMonitor::check_internet_connectivity() const {
    // Simple ping test (in real implementation, use proper network check)
    int result = system("ping -c 1 -W 1 8.8.8.8 > /dev/null 2>&1");
    return result == 0;
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
    std::cout << "🏠 [RPi4_Gateway] Created gateway: " << config_.gateway_id 
              << " at " << config_.location << std::endl;
}

RPi4_Gateway::~RPi4_Gateway() {
    stop();
    std::cout << "🏠 [RPi4_Gateway] Gateway destroyed" << std::endl;
}

bool RPi4_Gateway::initialize() {
    if (initialized_.load()) {
        std::cout << "⚠️ [RPi4_Gateway] Already initialized" << std::endl;
        return true;
    }
    
    std::cout << "🚀 [RPi4_Gateway] Initializing gateway..." << std::endl;
    std::cout << "   Gateway ID: " << config_.gateway_id << std::endl;
    std::cout << "   Location: " << config_.location << std::endl;
    std::cout << "   Mode: " << (config_.mode == GatewayMode::COLLECTOR_ONLY ? "Collector Only" :
                                config_.mode == GatewayMode::EDGE_PROCESSOR ? "Edge Processor" :
                                config_.mode == GatewayMode::HYBRID_BRIDGE ? "Hybrid Bridge" : "Failsafe") << std::endl;
    
    // Initialize components
    data_processor_ = std::make_unique<DataProcessor>(config_);
    if (!data_processor_->initialize()) {
        std::cerr << "❌ [RPi4_Gateway] Failed to initialize data processor" << std::endl;
        return false;
    }
    
    if (config_.enable_local_storage) {
        storage_manager_ = std::make_unique<StorageManager>(config_);
        if (!storage_manager_->initialize()) {
            std::cerr << "❌ [RPi4_Gateway] Failed to initialize storage manager" << std::endl;
            return false;
        }
    }
    
    system_monitor_ = std::make_unique<SystemMonitor>();
    if (!system_monitor_->start()) {
        std::cerr << "❌ [RPi4_Gateway] Failed to start system monitor" << std::endl;
        return false;
    }
    
    // Set up communication interfaces
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
    
    data_processor_->set_alert_callback(
        [this](const std::string& alert_type, const std::string& message) {
            this->handle_alert(alert_type, message);
        });
    
    initialized_ = true;
    std::cout << "✅ [RPi4_Gateway] Gateway initialized successfully" << std::endl;
    return true;
}

bool RPi4_Gateway::start() {
    if (!initialized_.load()) {
        std::cerr << "❌ [RPi4_Gateway] Gateway not initialized" << std::endl;
        return false;
    }
    
    if (running_.load()) {
        std::cout << "⚠️ [RPi4_Gateway] Gateway already running" << std::endl;
        return true;
    }
    
    running_ = true;
    
    // Start data processor
    if (!data_processor_->start()) {
        std::cerr << "❌ [RPi4_Gateway] Failed to start data processor" << std::endl;
        running_ = false;
        return false;
    }
    
    // Start communication interfaces
    for (auto& interface : comm_interfaces_) {
        if (!interface->start()) {
            std::cerr << "❌ [RPi4_Gateway] Failed to start " 
                      << interface->get_interface_name() << " interface" << std::endl;
        }
    }
    
    // Start main loop
    main_loop_thread_ = std::thread(&RPi4_Gateway::main_loop, this);
    
    std::cout << "🚀 [RPi4_Gateway] Gateway started successfully" << std::endl;
    std::cout << "   Active interfaces: " << comm_interfaces_.size() << std::endl;
    std::cout << "   Processing mode: " << (config_.processing_strategy == ProcessingStrategy::RAW_FORWARD ? "Raw Forward" :
                                           config_.processing_strategy == ProcessingStrategy::AGGREGATE_BATCH ? "Aggregate Batch" :
                                           config_.processing_strategy == ProcessingStrategy::SMART_FILTER ? "Smart Filter" : "Predictive Edge") << std::endl;
    
    return true;
}

void RPi4_Gateway::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "🛑 [RPi4_Gateway] Stopping gateway..." << std::endl;
    running_ = false;
    
    // Stop communication interfaces
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
    
    std::cout << "✅ [RPi4_Gateway] Gateway stopped gracefully" << std::endl;
}

void RPi4_Gateway::setup_communication_interfaces() {
    std::cout << "🔌 [RPi4_Gateway] Setting up communication interfaces..." << std::endl;
    
    // Create UART interface
    auto uart_interface = std::make_unique<UARTInterface>(config_.uart_device, config_.uart_baudrate);
    uart_interface->set_data_callback(
        [this](const SensorDataPacket& packet) {
            this->handle_sensor_data(packet);
        });
    
    if (uart_interface->initialize()) {
        comm_interfaces_.push_back(std::move(uart_interface));
        std::cout << "✅ [RPi4_Gateway] UART interface initialized" << std::endl;
    } else {
        std::cout << "⚠️ [RPi4_Gateway] UART interface initialization failed" << std::endl;
    }
    
    // Create SPI interface
    auto spi_interface = std::make_unique<SPIInterface>(config_.spi_device, config_.spi_speed);
    spi_interface->set_data_callback(
        [this](const SensorDataPacket& packet) {
            this->handle_sensor_data(packet);
        });
    
    if (spi_interface->initialize()) {
        comm_interfaces_.push_back(std::move(spi_interface));
        std::cout << "✅ [RPi4_Gateway] SPI interface initialized" << std::endl;
    } else {
        std::cout << "⚠️ [RPi4_Gateway] SPI interface initialization failed" << std::endl;
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
            std::cout << "✅ [RPi4_Gateway] I2C interface initialized with " 
                      << config_.i2c_addresses.size() << " addresses" << std::endl;
        } else {
            std::cout << "⚠️ [RPi4_Gateway] I2C interface initialization failed" << std::endl;
        }
    }
    
    std::cout << "🔌 [RPi4_Gateway] " << comm_interfaces_.size() 
              << " communication interfaces set up" << std::endl;
}

void RPi4_Gateway::main_loop() {
    std::cout << "🔄 [RPi4_Gateway] Main loop started" << std::endl;
    
    while (running_.load()) {
        // Periodic maintenance tasks
        
        // Update system status
        if (system_monitor_) {
            auto status = system_monitor_->get_system_status();
            
            // Log status periodically
            static auto last_status_log = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_status_log >= std::chrono::minutes(5)) {
                std::cout << "📊 [RPi4_Gateway] System Status:" << std::endl;
                std::cout << "   CPU: " << std::fixed << std::setprecision(1) 
                          << status.cpu_usage_percent << "%" << std::endl;
                std::cout << "   Memory: " << (status.memory_usage_bytes / 1024 / 1024) << " MB" << std::endl;
                std::cout << "   Disk: " << status.disk_usage_percent << "%" << std::endl;
                std::cout << "   Network: " << status.ip_address 
                          << (status.internet_connectivity ? " (Connected)" : " (Offline)") << std::endl;
                
                last_status_log = now;
            }
        }
        
        // Check for storage cleanup
        if (storage_manager_) {
            static auto last_cleanup = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_cleanup >= std::chrono::hours(1)) {
                storage_manager_->cleanup_old_data();
                storage_manager_->rotate_logs();
                last_cleanup = now;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    
    std::cout << "🏁 [RPi4_Gateway] Main loop finished" << std::endl;
}

void RPi4_Gateway::handle_sensor_data(const SensorDataPacket& packet) {
    std::cout << "📨 [RPi4_Gateway] Received sensor data from " << packet.sensor_id 
              << " via " << (packet.interface_used == CommInterface::UART_INTERFACE ? "UART" :
                           packet.interface_used == CommInterface::SPI_INTERFACE ? "SPI" :
                           packet.interface_used == CommInterface::I2C_INTERFACE ? "I2C" : "UNKNOWN") << std::endl;
    
    // Process through data processor
    if (data_processor_) {
        data_processor_->process_packet(packet);
    }
    
    // Store locally if enabled
    if (storage_manager_ && config_.enable_local_storage) {
        storage_manager_->store_sensor_data(packet);
    }
    
    // Integrate with thermal monitoring system
    integrate_with_thermal_system(packet);
}

void RPi4_Gateway::handle_mqtt_message(const std::string& topic, const std::string& message) {
    std::cout << "📤 [RPi4_Gateway] Forwarding MQTT message to topic: " << topic << std::endl;
    
    // Forward to external MQTT callback if set
    if (external_mqtt_callback_) {
        external_mqtt_callback_(topic, message);
    }
}

void RPi4_Gateway::handle_websocket_message(const std::string& message) {
    std::cout << "📤 [RPi4_Gateway] Forwarding WebSocket message" << std::endl;
    
    // Forward to external WebSocket callback if set
    if (external_websocket_callback_) {
        external_websocket_callback_(message);
    }
}

void RPi4_Gateway::handle_alert(const std::string& alert_type, const std::string& message) {
    std::cout << "🚨 [RPi4_Gateway] ALERT [" << alert_type << "]: " << message << std::endl;
    
    // Forward alert to MQTT
    if (external_mqtt_callback_) {
        std::string alert_topic = config_.mqtt_base_topic + "/alerts/" + alert_type;
        std::string alert_message = "{"
                                   "\"type\":\"" + alert_type + "\","
                                   "\"message\":\"" + message + "\","
                                   "\"gateway_id\":\"" + config_.gateway_id + "\","
                                   "\"timestamp\":" + std::to_string(
                                       std::chrono::duration_cast<std::chrono::seconds>(
                                           std::chrono::steady_clock::now().time_since_epoch()).count()) +
                                   "}";
        external_mqtt_callback_(alert_topic, alert_message);
    }
}

void RPi4_Gateway::integrate_with_thermal_system(const SensorDataPacket& packet) {
    // Integration with existing thermal monitoring system
    if (thermal_callback_ && packet.is_valid) {
        thermal_callback_(packet.sensor_id, packet.temperature_celsius, packet.humidity_percent);
    }
}

GatewayStatus RPi4_Gateway::get_status() const {
    if (system_monitor_) {
        auto status = system_monitor_->get_system_status();
        
        // Update gateway-specific status
        status.current_mode = config_.mode;
        status.processing_strategy = config_.processing_strategy;
        
        // Interface status
        status.uart_active = false;
        status.spi_active = false;
        status.i2c_active = false;
        
        for (const auto& interface : comm_interfaces_) {
            if (interface->get_interface_name() == "UART" && interface->is_active()) {
                status.uart_active = true;
            } else if (interface->get_interface_name() == "SPI" && interface->is_active()) {
                status.spi_active = true;
            } else if (interface->get_interface_name() == "I2C" && interface->is_active()) {
                status.i2c_active = true;
            }
        }
        
        // Get sensor count from data processor
        if (data_processor_) {
            auto stats = data_processor_->get_all_statistics();
            status.total_sensors_active = static_cast<uint32_t>(stats.size());
        }
        
        return status;
    }
    
    // Return minimal status if system monitor not available
    GatewayStatus status = {};
    status.is_running = running_.load();
    return status;
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

void RPi4_Gateway::set_external_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback) {
    external_mqtt_callback_ = callback;
}

void RPi4_Gateway::set_external_websocket_callback(std::function<void(const std::string&)> callback) {
    external_websocket_callback_ = callback;
}

void RPi4_Gateway::set_thermal_monitoring_callback(std::function<void(const std::string&, float, float)> callback) {
    thermal_callback_ = callback;
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
    
    // Home-friendly settings
    config.processing_interval_ms = 2000;
    config.aggregation_window_seconds = 300; // 5 minutes
    config.max_sensor_history = 500;
    config.enable_edge_analytics = true;
    config.enable_local_storage = true;
    
    // Conservative alert thresholds for home
    config.temp_alert_low = 15.0f;
    config.temp_alert_high = 30.0f;
    config.humidity_alert_high = 70.0f;
    
    // I2C addresses for common home sensors
    config.i2c_addresses = {0x76, 0x77, 0x44, 0x45}; // BME280, SHT30
    
    return config;
}

RPi4GatewayConfig create_industrial_gateway_config(const std::string& gateway_id) {
    RPi4GatewayConfig config;
    config.gateway_id = gateway_id;
    config.location = "Industrial";
    config.mode = GatewayMode::EDGE_PROCESSOR;
    config.processing_strategy = ProcessingStrategy::PREDICTIVE_EDGE;
    
    // High-performance settings
    config.processing_interval_ms = 500;
    config.aggregation_window_seconds = 60; // 1 minute
    config.max_sensor_history = 2000;
    config.enable_edge_analytics = true;
    config.enable_local_storage = true;
    config.worker_thread_count = 8;
    
    // Stricter alert thresholds for industrial
    config.temp_alert_low = 5.0f;
    config.temp_alert_high = 40.0f;
    config.humidity_alert_high = 80.0f;
    config.packet_loss_alert_threshold = 0.05f; // 5%
    
    // More I2C addresses for industrial sensors
    config.i2c_addresses = {0x76, 0x77, 0x44, 0x45, 0x48, 0x49, 0x4A, 0x4B};
    
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

} // namespace gateway_factory

} // namespace rpi4_gateway 