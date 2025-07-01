#include "STM32_SensorNode.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <fstream>

namespace stm32_simulation {

//=============================================================================
// STM32_SensorNode Implementation
//=============================================================================

STM32_SensorNode::STM32_SensorNode(const SensorNodeConfig& config)
    : config_(config), running_(false), initialized_(false),
      random_generator_(std::random_device{}()),
      temp_noise_(0.0f, config.noise_level),
      humidity_noise_(0.0f, config.noise_level * 2.0f),
      fault_dist_(0.0f, 1.0f),
      current_base_temp_(config.base_temperature),
      current_base_humidity_(config.base_humidity),
      temp_trend_(0.0f), humidity_trend_(0.0f),
      sensor_fault_(false), connection_fault_(false),
      sensor_drift_(0.0f), supply_voltage_(3.3f),
      reading_count_(0), transmission_count_(0) {
    
    reading_history_.reserve(MAX_HISTORY);
    start_time_ = std::chrono::steady_clock::now();
    
    std::cout << "🔧 [" << config_.node_id << "] STM32 " << get_sensor_type_string() 
              << " sensor node created at " << config_.location << std::endl;
}

STM32_SensorNode::~STM32_SensorNode() {
    stop();
    std::cout << "🏁 [" << config_.node_id << "] STM32 sensor node destroyed" << std::endl;
}

bool STM32_SensorNode::initialize() {
    if (initialized_.load()) {
        return true;
    }
    
    std::cout << "🚀 [" << config_.node_id << "] Initializing STM32 sensor node..." << std::endl;
    std::cout << "   Sensor Type: " << get_sensor_type_string() << std::endl;
    std::cout << "   Communication: " << get_comm_protocol_string() << std::endl;
    std::cout << "   Environment: " << get_environment_pattern_string() << std::endl;
    std::cout << "   Reading Interval: " << config_.reading_interval_ms << "ms" << std::endl;
    std::cout << "   Transmission Interval: " << config_.transmission_interval_ms << "ms" << std::endl;
    
    // Simulate hardware initialization
    supply_voltage_ = simulate_supply_voltage();
    sensor_drift_ = 0.0f;
    
    // Reset counters
    reading_count_ = 0;
    transmission_count_ = 0;
    
    initialized_ = true;
    std::cout << "✅ [" << config_.node_id << "] STM32 sensor node initialized" << std::endl;
    return true;
}

bool STM32_SensorNode::start() {
    if (!initialized_.load()) {
        std::cerr << "❌ [" << config_.node_id << "] Cannot start: node not initialized" << std::endl;
        return false;
    }
    
    if (running_.load()) {
        std::cout << "⚠️ [" << config_.node_id << "] Node already running" << std::endl;
        return true;
    }
    
    running_ = true;
    
    // Start sensor reading thread
    sensor_thread_ = std::thread(&STM32_SensorNode::sensor_reading_loop, this);
    
    // Start transmission thread
    transmission_thread_ = std::thread(&STM32_SensorNode::transmission_loop, this);
    
    std::cout << "🚀 [" << config_.node_id << "] STM32 sensor node started" << std::endl;
    return true;
}

void STM32_SensorNode::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "🛑 [" << config_.node_id << "] Stopping STM32 sensor node..." << std::endl;
    running_ = false;
    
    // Wait for threads to finish
    if (sensor_thread_.joinable()) {
        sensor_thread_.join();
    }
    if (transmission_thread_.joinable()) {
        transmission_thread_.join();
    }
    
    std::cout << "✅ [" << config_.node_id << "] STM32 sensor node stopped gracefully" << std::endl;
}

std::string STM32_SensorNode::get_status() const {
    std::stringstream ss;
    ss << "[" << config_.node_id << "] ";
    ss << (running_.load() ? "RUNNING" : "STOPPED");
    ss << " | Readings: " << reading_count_;
    ss << " | Transmissions: " << transmission_count_;
    ss << " | Supply: " << std::fixed << std::setprecision(2) << supply_voltage_ << "V";
    
    if (sensor_fault_) ss << " | SENSOR_FAULT";
    if (connection_fault_) ss << " | CONN_FAULT";
    
    return ss.str();
}

SensorReading STM32_SensorNode::get_last_reading() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return last_reading_;
}

std::vector<SensorReading> STM32_SensorNode::get_reading_history(int count) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<SensorReading> result;
    int start_index = std::max(0, static_cast<int>(reading_history_.size()) - count);
    
    for (int i = start_index; i < static_cast<int>(reading_history_.size()); ++i) {
        result.push_back(reading_history_[i]);
    }
    
    return result;
}

void STM32_SensorNode::set_uart_callback(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback) {
    uart_callback_ = callback;
}

void STM32_SensorNode::set_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback) {
    mqtt_callback_ = callback;
}

void STM32_SensorNode::inject_fault() {
    sensor_fault_ = true;
    std::cout << "🚨 [" << config_.node_id << "] Fault injected" << std::endl;
}

void STM32_SensorNode::simulate_power_loss(int duration_ms) {
    std::cout << "⚡ [" << config_.node_id << "] Simulating power loss for " << duration_ms << "ms" << std::endl;
    
    // Temporarily stop the node
    bool was_running = running_.load();
    if (was_running) {
        running_ = false;
        
        // Wait for the specified duration
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        
        // Restart
        running_ = true;
        supply_voltage_ = simulate_supply_voltage(); // Voltage may change after power cycle
    }
    
    std::cout << "🔋 [" << config_.node_id << "] Power restored" << std::endl;
}

void STM32_SensorNode::change_environment(EnvironmentPattern new_pattern) {
    config_.environment = new_pattern;
    std::cout << "🌡️ [" << config_.node_id << "] Environment changed to: " 
              << get_environment_pattern_string() << std::endl;
}

void STM32_SensorNode::update_base_conditions(float temp, float humidity) {
    current_base_temp_ = temp;
    current_base_humidity_ = humidity;
    std::cout << "📊 [" << config_.node_id << "] Base conditions updated: " 
              << temp << "°C, " << humidity << "%" << std::endl;
}

//=============================================================================
// Private Methods - Threading
//=============================================================================

void STM32_SensorNode::sensor_reading_loop() {
    std::cout << "🔄 [" << config_.node_id << "] Sensor reading loop started" << std::endl;
    
    while (running_.load()) {
        // Read sensor
        SensorReading reading = read_sensor();
        
        // Store reading
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            last_reading_ = reading;
            reading_history_.push_back(reading);
            
            // Limit history size
            if (reading_history_.size() > MAX_HISTORY) {
                reading_history_.erase(reading_history_.begin());
            }
        }
        
        reading_count_++;
        
        if (reading.is_valid) {
            std::cout << "📊 [" << config_.node_id << "] T: " 
                      << std::fixed << std::setprecision(1) << reading.temperature_celsius 
                      << "°C, H: " << reading.humidity_percent 
                      << "%, V: " << std::setprecision(2) << reading.supply_voltage << "V"
                      << " (ADC: " << reading.raw_temp_adc << "/" << reading.raw_humidity_adc << ")"
                      << std::endl;
        } else {
            std::cout << "❌ [" << config_.node_id << "] Invalid sensor reading" << std::endl;
        }
        
        // Sleep until next reading
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.reading_interval_ms));
    }
    
    std::cout << "🏁 [" << config_.node_id << "] Sensor reading loop finished" << std::endl;
}

void STM32_SensorNode::transmission_loop() {
    std::cout << "📡 [" << config_.node_id << "] Transmission loop started" << std::endl;
    
    while (running_.load()) {
        // Check if we have data to transmit
        SensorReading reading;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            reading = last_reading_;
        }
        
        // Check connection status
        if (check_connection_fault()) {
            std::cout << "📶 [" << config_.node_id << "] Connection fault - transmission skipped" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.transmission_interval_ms));
            continue;
        }
        
        // Transmit based on communication protocol
        if (reading.is_valid) {
            switch (config_.comm_protocol) {
                case CommProtocol::UART_TO_GATEWAY:
                case CommProtocol::SPI_TO_GATEWAY:
                case CommProtocol::I2C_TO_GATEWAY: {
                    if (uart_callback_) {
                        std::vector<uint8_t> packet = create_binary_packet(reading);
                        uart_callback_(config_.node_id, packet);
                        std::cout << "📤 [" << config_.node_id << "] Data sent via " 
                                  << get_comm_protocol_string() << " (" << packet.size() << " bytes)" << std::endl;
                    }
                    break;
                }
                
                case CommProtocol::MQTT_DIRECT: {
                    if (mqtt_callback_) {
                        std::string message = format_mqtt_message(reading);
                        std::string topic = "sensors/" + config_.node_id + "/data";
                        mqtt_callback_(topic, message);
                        std::cout << "📤 [" << config_.node_id << "] Data sent via MQTT to topic: " 
                                  << topic << std::endl;
                    }
                    break;
                }
            }
            
            transmission_count_++;
        }
        
        // Sleep until next transmission
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.transmission_interval_ms));
    }
    
    std::cout << "🏁 [" << config_.node_id << "] Transmission loop finished" << std::endl;
}

//=============================================================================
// Private Methods - Sensor Simulation
//=============================================================================

SensorReading STM32_SensorNode::read_sensor() {
    SensorReading reading = {};
    reading.timestamp = std::chrono::steady_clock::now();
    reading.supply_voltage = simulate_supply_voltage();
    reading.sensor_status = get_sensor_status();
    
    // Check for sensor faults
    reading.is_valid = !check_sensor_fault();
    
    if (reading.is_valid) {
        // Simulate sensor readings
        reading.temperature_celsius = simulate_temperature();
        reading.humidity_percent = simulate_humidity();
        
        // Convert to ADC values (simulate hardware)
        reading.raw_temp_adc = temperature_to_adc(reading.temperature_celsius);
        reading.raw_humidity_adc = humidity_to_adc(reading.humidity_percent);
        
        // BME280 also has pressure
        if (config_.sensor_type == SensorType::BME280) {
            reading.pressure_hpa = 1013.25f + temp_noise_(random_generator_) * 10.0f;
        }
        
        // Apply sensor drift over time
        sensor_drift_ += config_.drift_rate * (fault_dist_(random_generator_) - 0.5f);
        reading.temperature_celsius += sensor_drift_;
    } else {
        // Invalid reading - set to error values
        reading.temperature_celsius = -999.0f;
        reading.humidity_percent = -999.0f;
        reading.raw_temp_adc = 0xFFFF;
        reading.raw_humidity_adc = 0xFFFF;
    }
    
    return reading;
}

float STM32_SensorNode::simulate_temperature() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - start_time_;
    float hours = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 3600000.0f;
    
    // Apply environmental pattern
    float temp = apply_environmental_pattern(current_base_temp_, hours, true);
    
    // Add sensor noise
    temp += temp_noise_(random_generator_);
    
    // Apply sensor-specific characteristics
    switch (config_.sensor_type) {
        case SensorType::DHT22:
            temp = std::round(temp * 10.0f) / 10.0f; // 0.1°C resolution
            break;
        case SensorType::DS18B20:
            temp = std::round(temp * 16.0f) / 16.0f; // 0.0625°C resolution
            break;
        case SensorType::BME280:
            temp = std::round(temp * 100.0f) / 100.0f; // 0.01°C resolution
            break;
        case SensorType::SHT30:
            temp = std::round(temp * 100.0f) / 100.0f; // 0.01°C resolution
            break;
        case SensorType::FAULTY_SENSOR:
            if (fault_dist_(random_generator_) < 0.1f) {
                temp += (fault_dist_(random_generator_) - 0.5f) * 50.0f; // Random spikes
            }
            break;
        default:
            break;
    }
    
    return temp;
}

float STM32_SensorNode::simulate_humidity() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - start_time_;
    float hours = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 3600000.0f;
    
    // Apply environmental pattern
    float humidity = apply_environmental_pattern(current_base_humidity_, hours, false);
    
    // Add sensor noise
    humidity += humidity_noise_(random_generator_);
    
    // Clamp to valid range
    humidity = std::max(0.0f, std::min(100.0f, humidity));
    
    // Apply sensor-specific characteristics
    switch (config_.sensor_type) {
        case SensorType::DHT22:
            humidity = std::round(humidity * 10.0f) / 10.0f; // 0.1% resolution
            break;
        case SensorType::BME280:
            humidity = std::round(humidity * 1024.0f) / 1024.0f; // High resolution
            break;
        case SensorType::SHT30:
            humidity = std::round(humidity * 100.0f) / 100.0f; // 0.01% resolution
            break;
        case SensorType::DS18B20:
            humidity = 0.0f; // No humidity sensor
            break;
        case SensorType::FAULTY_SENSOR:
            if (fault_dist_(random_generator_) < 0.1f) {
                humidity = fault_dist_(random_generator_) * 120.0f; // Invalid readings
            }
            break;
        default:
            break;
    }
    
    return humidity;
}

float STM32_SensorNode::apply_environmental_pattern(float base_value, float time_hours, bool is_temperature) {
    float result = base_value;
    float variation = is_temperature ? config_.temp_variation : config_.humidity_variation;
    
    switch (config_.environment) {
        case EnvironmentPattern::INDOOR_STABLE:
            // Small random variations around base value
            result += std::sin(time_hours * 0.1f) * variation * 0.3f;
            break;
            
        case EnvironmentPattern::OUTDOOR_VARIABLE:
            // Larger variations with daily cycles
            result += std::sin(time_hours * M_PI / 12.0f) * variation; // Daily cycle
            result += std::sin(time_hours * M_PI / 6.0f) * variation * 0.3f; // Weather variation
            break;
            
        case EnvironmentPattern::HEATING_CYCLE:
            if (is_temperature) {
                // Heating cycles every 2 hours
                float cycle = std::fmod(time_hours, 2.0f);
                if (cycle < 1.0f) {
                    result += variation * (1.0f - cycle); // Heating on
                } else {
                    result -= variation * (cycle - 1.0f); // Heating off
                }
            }
            break;
            
        case EnvironmentPattern::COOLING_CYCLE:
            if (is_temperature) {
                // Cooling cycles every 1.5 hours
                float cycle = std::fmod(time_hours, 1.5f);
                if (cycle < 0.75f) {
                    result -= variation * cycle; // Cooling on
                } else {
                    result += variation * (cycle - 0.75f); // Cooling off
                }
            }
            break;
            
        case EnvironmentPattern::INDUSTRIAL:
            // Sharp changes due to machinery
            result += std::sin(time_hours * 2.0f) * variation * 1.5f;
            if (fault_dist_(random_generator_) < 0.05f) {
                result += (fault_dist_(random_generator_) - 0.5f) * variation * 3.0f;
            }
            break;
            
        case EnvironmentPattern::GREENHOUSE:
            // Controlled environment with irrigation cycles
            result += std::sin(time_hours * M_PI / 8.0f) * variation * 0.5f; // 16-hour cycle
            if (!is_temperature) {
                // Humidity spikes during watering
                if (std::fmod(time_hours, 4.0f) < 0.5f) {
                    result += variation * 2.0f;
                }
            }
            break;
            
        case EnvironmentPattern::SERVER_ROOM:
            if (is_temperature) {
                // Stable but slightly elevated
                result += 3.0f + std::sin(time_hours * 0.2f) * 1.0f;
            } else {
                // Low humidity
                result *= 0.6f;
            }
            break;
            
        case EnvironmentPattern::BASEMENT:
            if (is_temperature) {
                result -= 3.0f; // Cooler
            } else {
                result += 15.0f; // More humid
            }
            result += std::sin(time_hours * 0.05f) * variation * 0.2f; // Very slow changes
            break;
            
        case EnvironmentPattern::ATTIC:
            if (is_temperature) {
                result += 5.0f; // Warmer
                result += std::sin(time_hours * M_PI / 12.0f) * variation * 2.0f; // Strong daily cycle
            }
            break;
    }
    
    return result;
}

bool STM32_SensorNode::check_sensor_fault() {
    // Temporary fault injection
    if (sensor_fault_) {
        // Clear fault after some time
        if (fault_dist_(random_generator_) < 0.1f) {
            sensor_fault_ = false;
            std::cout << "🔧 [" << config_.node_id << "] Sensor fault cleared" << std::endl;
        }
        return true;
    }
    
    // Random faults based on configuration
    if (fault_dist_(random_generator_) < config_.fault_probability) {
        std::cout << "⚠️ [" << config_.node_id << "] Random sensor fault occurred" << std::endl;
        return true;
    }
    
    // Intermittent sensor behavior
    if (config_.sensor_type == SensorType::INTERMITTENT) {
        return fault_dist_(random_generator_) < 0.3f; // 30% failure rate
    }
    
    return false;
}

bool STM32_SensorNode::check_connection_fault() {
    // Temporary connection fault
    if (connection_fault_) {
        // Clear fault after some time
        if (fault_dist_(random_generator_) < 0.2f) {
            connection_fault_ = false;
            std::cout << "📶 [" << config_.node_id << "] Connection restored" << std::endl;
        }
        return true;
    }
    
    // Random connection issues
    if (fault_dist_(random_generator_) > config_.connection_stability) {
        connection_fault_ = true;
        std::cout << "📶 [" << config_.node_id << "] Connection fault detected" << std::endl;
        return true;
    }
    
    return false;
}

//=============================================================================
// Private Methods - Data Formatting
//=============================================================================

std::string STM32_SensorNode::format_uart_message(const SensorReading& reading) {
    std::stringstream ss;
    ss << "STM32:" << config_.node_id << ","
       << "T:" << std::fixed << std::setprecision(2) << reading.temperature_celsius << ","
       << "H:" << reading.humidity_percent << ","
       << "V:" << reading.supply_voltage << ","
       << "S:" << std::hex << static_cast<int>(reading.sensor_status)
       << std::dec << "\n";
    return ss.str();
}

std::string STM32_SensorNode::format_mqtt_message(const SensorReading& reading) {
    std::stringstream ss;
    ss << "{"
       << "\"temperature\":" << reading.temperature_celsius << ","
       << "\"humidity\":" << reading.humidity_percent << ","
       << "\"location\":\"" << config_.location << "\","
       << "\"node_id\":\"" << config_.node_id << "\","
       << "\"supply_voltage\":" << reading.supply_voltage << ","
       << "\"sensor_status\":" << static_cast<int>(reading.sensor_status) << ","
       << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
              reading.timestamp.time_since_epoch()).count()
       << "}";
    return ss.str();
}

std::vector<uint8_t> STM32_SensorNode::create_binary_packet(const SensorReading& reading) {
    std::vector<uint8_t> packet;
    
    // Simple binary protocol:
    // [Header(2)] [NodeID(4)] [Temp(4)] [Humidity(4)] [Voltage(2)] [Status(1)] [Checksum(1)]
    
    // Header
    packet.push_back(0xAA);
    packet.push_back(0xBB);
    
    // Node ID (simplified - use hash)
    uint32_t node_hash = std::hash<std::string>{}(config_.node_id);
    packet.push_back((node_hash >> 24) & 0xFF);
    packet.push_back((node_hash >> 16) & 0xFF);
    packet.push_back((node_hash >> 8) & 0xFF);
    packet.push_back(node_hash & 0xFF);
    
    // Temperature (as int16 * 100)
    int16_t temp_int = static_cast<int16_t>(reading.temperature_celsius * 100);
    packet.push_back((temp_int >> 8) & 0xFF);
    packet.push_back(temp_int & 0xFF);
    
    // Humidity (as uint16 * 100)
    uint16_t hum_int = static_cast<uint16_t>(reading.humidity_percent * 100);
    packet.push_back((hum_int >> 8) & 0xFF);
    packet.push_back(hum_int & 0xFF);
    
    // Supply voltage (as uint16 * 1000)
    uint16_t volt_int = static_cast<uint16_t>(reading.supply_voltage * 1000);
    packet.push_back((volt_int >> 8) & 0xFF);
    packet.push_back(volt_int & 0xFF);
    
    // Status
    packet.push_back(reading.sensor_status);
    
    // Simple checksum
    uint8_t checksum = 0;
    for (size_t i = 2; i < packet.size(); ++i) {
        checksum ^= packet[i];
    }
    packet.push_back(checksum);
    
    return packet;
}

//=============================================================================
// Private Methods - Hardware Simulation
//=============================================================================

uint16_t STM32_SensorNode::temperature_to_adc(float temp_celsius) {
    // Simulate 12-bit ADC conversion
    // Typical range: -40°C to +85°C -> 0 to 4095
    float normalized = (temp_celsius + 40.0f) / 125.0f;
    normalized = std::max(0.0f, std::min(1.0f, normalized));
    return static_cast<uint16_t>(normalized * 4095.0f);
}

uint16_t STM32_SensorNode::humidity_to_adc(float humidity_percent) {
    // Simulate 12-bit ADC conversion
    // Range: 0% to 100% -> 0 to 4095
    float normalized = humidity_percent / 100.0f;
    normalized = std::max(0.0f, std::min(1.0f, normalized));
    return static_cast<uint16_t>(normalized * 4095.0f);
}

float STM32_SensorNode::simulate_supply_voltage() {
    // Simulate supply voltage variation
    float base_voltage = 3.3f;
    float variation = config_.power_variation * (fault_dist_(random_generator_) - 0.5f);
    return base_voltage + variation;
}

uint8_t STM32_SensorNode::get_sensor_status() {
    uint8_t status = 0x00;
    
    if (sensor_fault_) status |= 0x01;          // Sensor fault bit
    if (connection_fault_) status |= 0x02;      // Connection fault bit
    if (supply_voltage_ < 3.0f) status |= 0x04; // Low voltage bit
    if (reading_count_ > 0) status |= 0x80;     // Data ready bit
    
    return status;
}

//=============================================================================
// Private Methods - Utility
//=============================================================================

std::string STM32_SensorNode::get_sensor_type_string() const {
    switch (config_.sensor_type) {
        case SensorType::DHT22: return "DHT22";
        case SensorType::DS18B20: return "DS18B20";
        case SensorType::BME280: return "BME280";
        case SensorType::SHT30: return "SHT30";
        case SensorType::FAULTY_SENSOR: return "FAULTY";
        case SensorType::INTERMITTENT: return "INTERMITTENT";
        default: return "UNKNOWN";
    }
}

std::string STM32_SensorNode::get_comm_protocol_string() const {
    switch (config_.comm_protocol) {
        case CommProtocol::UART_TO_GATEWAY: return "UART";
        case CommProtocol::MQTT_DIRECT: return "MQTT";
        case CommProtocol::SPI_TO_GATEWAY: return "SPI";
        case CommProtocol::I2C_TO_GATEWAY: return "I2C";
        default: return "UNKNOWN";
    }
}

std::string STM32_SensorNode::get_environment_pattern_string() const {
    switch (config_.environment) {
        case EnvironmentPattern::INDOOR_STABLE: return "Indoor Stable";
        case EnvironmentPattern::OUTDOOR_VARIABLE: return "Outdoor Variable";
        case EnvironmentPattern::HEATING_CYCLE: return "Heating Cycle";
        case EnvironmentPattern::COOLING_CYCLE: return "Cooling Cycle";
        case EnvironmentPattern::INDUSTRIAL: return "Industrial";
        case EnvironmentPattern::GREENHOUSE: return "Greenhouse";
        case EnvironmentPattern::SERVER_ROOM: return "Server Room";
        case EnvironmentPattern::BASEMENT: return "Basement";
        case EnvironmentPattern::ATTIC: return "Attic";
        default: return "Unknown";
    }
}

//=============================================================================
// SensorDeployment Implementation
//=============================================================================

SensorDeployment::~SensorDeployment() {
    stop_all();
}

void SensorDeployment::add_sensor_node(std::unique_ptr<STM32_SensorNode> node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Set up callbacks if global ones are available
    if (global_uart_callback_) {
        node->set_uart_callback(global_uart_callback_);
    }
    if (global_mqtt_callback_) {
        node->set_mqtt_callback(global_mqtt_callback_);
    }
    
    sensor_nodes_.push_back(std::move(node));
    std::cout << "➕ Added sensor node to deployment (Total: " << sensor_nodes_.size() << ")" << std::endl;
}

void SensorDeployment::remove_sensor_node(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = std::remove_if(sensor_nodes_.begin(), sensor_nodes_.end(),
        [&node_id](const std::unique_ptr<STM32_SensorNode>& node) {
            return node->get_node_id() == node_id;
        });
    
    if (it != sensor_nodes_.end()) {
        sensor_nodes_.erase(it, sensor_nodes_.end());
        std::cout << "➖ Removed sensor node " << node_id << " from deployment" << std::endl;
    }
}

bool SensorDeployment::start_all() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::cout << "🚀 Starting sensor deployment (" << sensor_nodes_.size() << " nodes)..." << std::endl;
    
    bool all_started = true;
    for (auto& node : sensor_nodes_) {
        if (!node->initialize() || !node->start()) {
            all_started = false;
        }
    }
    
    if (all_started) {
        std::cout << "✅ All sensor nodes started successfully" << std::endl;
    } else {
        std::cout << "⚠️ Some sensor nodes failed to start" << std::endl;
    }
    
    return all_started;
}

void SensorDeployment::stop_all() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::cout << "🛑 Stopping sensor deployment..." << std::endl;
    
    for (auto& node : sensor_nodes_) {
        node->stop();
    }
    
    std::cout << "✅ All sensor nodes stopped" << std::endl;
}

std::vector<std::string> SensorDeployment::get_node_ids() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<std::string> ids;
    for (const auto& node : sensor_nodes_) {
        ids.push_back(node->get_node_id());
    }
    return ids;
}

std::string SensorDeployment::get_deployment_status() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::stringstream ss;
    ss << "Sensor Deployment Status (" << sensor_nodes_.size() << " nodes):\n";
    
    for (const auto& node : sensor_nodes_) {
        ss << "  " << node->get_status() << "\n";
    }
    
    return ss.str();
}

void SensorDeployment::simulate_power_outage(int duration_ms) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::cout << "⚡ Simulating power outage for all nodes (" << duration_ms << "ms)" << std::endl;
    
    for (auto& node : sensor_nodes_) {
        node->simulate_power_loss(duration_ms);
    }
}

void SensorDeployment::change_all_environments(EnvironmentPattern pattern) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    for (auto& node : sensor_nodes_) {
        node->change_environment(pattern);
    }
}

void SensorDeployment::inject_random_faults(float fault_rate) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    for (auto& node : sensor_nodes_) {
        if (dist(gen) < fault_rate) {
            node->inject_fault();
        }
    }
}

std::vector<SensorReading> SensorDeployment::collect_all_readings() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<SensorReading> all_readings;
    for (const auto& node : sensor_nodes_) {
        auto reading = node->get_last_reading();
        all_readings.push_back(reading);
    }
    return all_readings;
}

void SensorDeployment::save_deployment_log(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    
    file << get_deployment_status();
    
    file << "\nDetailed Readings:\n";
    for (const auto& node : sensor_nodes_) {
        auto readings = node->get_reading_history(10);
        file << "\n" << node->get_node_id() << " (Last 10 readings):\n";
        for (const auto& reading : readings) {
            if (reading.is_valid) {
                file << "  T: " << reading.temperature_celsius 
                     << "°C, H: " << reading.humidity_percent << "%\n";
            }
        }
    }
    
    std::cout << "📄 Deployment log saved to: " << filename << std::endl;
}

void SensorDeployment::set_global_uart_callback(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback) {
    global_uart_callback_ = callback;
    
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    for (auto& node : sensor_nodes_) {
        node->set_uart_callback(callback);
    }
}

void SensorDeployment::set_global_mqtt_callback(std::function<void(const std::string&, const std::string&)> callback) {
    global_mqtt_callback_ = callback;
    
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    for (auto& node : sensor_nodes_) {
        node->set_mqtt_callback(callback);
    }
}

//=============================================================================
// Factory Functions
//=============================================================================

namespace sensor_factory {

SensorNodeConfig create_indoor_node(const std::string& id, const std::string& location) {
    SensorNodeConfig config;
    config.node_id = id;
    config.location = location;
    config.sensor_type = SensorType::DHT22;
    config.comm_protocol = CommProtocol::UART_TO_GATEWAY;
    config.environment = EnvironmentPattern::INDOOR_STABLE;
    config.base_temperature = 22.0f;
    config.base_humidity = 45.0f;
    config.temp_variation = 1.0f;
    config.humidity_variation = 3.0f;
    config.reading_interval_ms = 2000;
    config.transmission_interval_ms = 5000;
    return config;
}

SensorNodeConfig create_outdoor_node(const std::string& id, const std::string& location) {
    SensorNodeConfig config;
    config.node_id = id;
    config.location = location;
    config.sensor_type = SensorType::BME280;
    config.comm_protocol = CommProtocol::MQTT_DIRECT;
    config.environment = EnvironmentPattern::OUTDOOR_VARIABLE;
    config.base_temperature = 18.0f;
    config.base_humidity = 60.0f;
    config.temp_variation = 5.0f;
    config.humidity_variation = 15.0f;
    config.reading_interval_ms = 3000;
    config.transmission_interval_ms = 10000;
    return config;
}

SensorNodeConfig create_industrial_node(const std::string& id, const std::string& location) {
    SensorNodeConfig config;
    config.node_id = id;
    config.location = location;
    config.sensor_type = SensorType::SHT30;
    config.comm_protocol = CommProtocol::SPI_TO_GATEWAY;
    config.environment = EnvironmentPattern::INDUSTRIAL;
    config.base_temperature = 25.0f;
    config.base_humidity = 40.0f;
    config.temp_variation = 8.0f;
    config.humidity_variation = 10.0f;
    config.fault_probability = 0.005f; // Higher fault rate
    config.reading_interval_ms = 1000;
    config.transmission_interval_ms = 3000;
    return config;
}

SensorNodeConfig create_greenhouse_node(const std::string& id, const std::string& location) {
    SensorNodeConfig config;
    config.node_id = id;
    config.location = location;
    config.sensor_type = SensorType::SHT30;
    config.comm_protocol = CommProtocol::UART_TO_GATEWAY;
    config.environment = EnvironmentPattern::GREENHOUSE;
    config.base_temperature = 26.0f;
    config.base_humidity = 70.0f;
    config.temp_variation = 3.0f;
    config.humidity_variation = 20.0f;
    config.reading_interval_ms = 1500;
    config.transmission_interval_ms = 4000;
    return config;
}

SensorNodeConfig create_server_room_node(const std::string& id, const std::string& location) {
    SensorNodeConfig config;
    config.node_id = id;
    config.location = location;
    config.sensor_type = SensorType::BME280;
    config.comm_protocol = CommProtocol::I2C_TO_GATEWAY;
    config.environment = EnvironmentPattern::SERVER_ROOM;
    config.base_temperature = 24.0f;
    config.base_humidity = 35.0f;
    config.temp_variation = 1.5f;
    config.humidity_variation = 5.0f;
    config.reading_interval_ms = 1000;
    config.transmission_interval_ms = 2000;
    return config;
}

std::unique_ptr<SensorDeployment> create_home_deployment() {
    auto deployment = std::make_unique<SensorDeployment>();
    
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_indoor_node("home_living", "Living Room")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_indoor_node("home_kitchen", "Kitchen")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_indoor_node("home_bedroom", "Master Bedroom")));
    
    // Add basement and attic with special configurations
    auto basement_config = create_indoor_node("home_basement", "Basement");
    basement_config.environment = EnvironmentPattern::BASEMENT;
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(basement_config));
    
    auto attic_config = create_indoor_node("home_attic", "Attic");
    attic_config.environment = EnvironmentPattern::ATTIC;
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(attic_config));
    
    return deployment;
}

std::unique_ptr<SensorDeployment> create_office_deployment() {
    auto deployment = std::make_unique<SensorDeployment>();
    
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_indoor_node("office_lobby", "Main Lobby")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_indoor_node("office_conf1", "Conference Room 1")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_indoor_node("office_conf2", "Conference Room 2")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_server_room_node("office_server", "Server Room")));
    
    return deployment;
}

std::unique_ptr<SensorDeployment> create_industrial_deployment() {
    auto deployment = std::make_unique<SensorDeployment>();
    
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_industrial_node("factory_floor1", "Production Floor 1")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_industrial_node("factory_floor2", "Production Floor 2")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_industrial_node("factory_storage", "Storage Area")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_industrial_node("factory_office", "Factory Office")));
    
    return deployment;
}

std::unique_ptr<SensorDeployment> create_agricultural_deployment() {
    auto deployment = std::make_unique<SensorDeployment>();
    
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_greenhouse_node("greenhouse_1", "Greenhouse Section 1")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_greenhouse_node("greenhouse_2", "Greenhouse Section 2")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_outdoor_node("field_north", "North Field")));
    deployment->add_sensor_node(std::make_unique<STM32_SensorNode>(create_outdoor_node("field_south", "South Field")));
    
    return deployment;
}

} // namespace sensor_factory

} // namespace stm32_simulation 