#include "RPi4_Gateway.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <cmath>

// Linux system headers for hardware interfaces
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

namespace rpi4_gateway {

//=============================================================================
// UART Interface Implementation
//=============================================================================

UARTInterface::UARTInterface(const std::string& device, int baudrate)
    : device_(device), baudrate_(baudrate), fd_(-1), active_(false) {
    std::cout << "🔌 [UART] Interface created for device: " << device_ 
              << " @ " << baudrate_ << " baud" << std::endl;
}

UARTInterface::~UARTInterface() {
    stop();
    if (fd_ >= 0) {
        close(fd_);
    }
    std::cout << "🔌 [UART] Interface destroyed" << std::endl;
}

bool UARTInterface::initialize() {
    std::cout << "🚀 [UART] Initializing interface..." << std::endl;
    
    // Open UART device
    fd_ = open(device_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "❌ [UART] Failed to open device: " << device_ << std::endl;
        return false;
    }
    
    // Configure UART
    struct termios tty;
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "❌ [UART] Failed to get device attributes" << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Set baud rate
    speed_t speed;
    switch (baudrate_) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        default: speed = B115200; break;
    }
    
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    
    // Configure 8N1
    tty.c_cflag &= ~PARENB;   // No parity
    tty.c_cflag &= ~CSTOPB;   // One stop bit
    tty.c_cflag &= ~CSIZE;    // Clear size bits
    tty.c_cflag |= CS8;       // 8 data bits
    tty.c_cflag &= ~CRTSCTS;  // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable reading and ignore modem controls
    
    // Configure input modes
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // No software flow control
    tty.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    
    // Configure output modes
    tty.c_oflag &= ~OPOST; // Raw output
    
    // Set timeouts
    tty.c_cc[VMIN] = 0;   // Non-blocking read
    tty.c_cc[VTIME] = 10; // 1 second timeout
    
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "❌ [UART] Failed to set device attributes" << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }
    
    std::cout << "✅ [UART] Interface initialized successfully" << std::endl;
    return true;
}

bool UARTInterface::start() {
    if (fd_ < 0) {
        std::cerr << "❌ [UART] Interface not initialized" << std::endl;
        return false;
    }
    
    if (active_.load()) {
        std::cout << "⚠️ [UART] Interface already running" << std::endl;
        return true;
    }
    
    active_ = true;
    reader_thread_ = std::thread(&UARTInterface::reader_loop, this);
    
    std::cout << "🚀 [UART] Interface started" << std::endl;
    return true;
}

void UARTInterface::stop() {
    if (!active_.load()) {
        return;
    }
    
    std::cout << "🛑 [UART] Stopping interface..." << std::endl;
    active_ = false;
    
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
    
    std::cout << "✅ [UART] Interface stopped" << std::endl;
}

void UARTInterface::set_data_callback(std::function<void(const SensorDataPacket&)> callback) {
    data_callback_ = callback;
}

void UARTInterface::reader_loop() {
    std::cout << "🔄 [UART] Reader thread started" << std::endl;
    
    std::vector<uint8_t> buffer;
    buffer.reserve(1024);
    
    while (active_.load()) {
        uint8_t temp_buffer[256];
        ssize_t bytes_read = read(fd_, temp_buffer, sizeof(temp_buffer));
        
        if (bytes_read > 0) {
            // Add to buffer
            buffer.insert(buffer.end(), temp_buffer, temp_buffer + bytes_read);
            
            // Look for complete packets (simple framing: 0xAA 0xBB ... checksum)
            while (buffer.size() >= 14) { // Minimum packet size
                // Find start of packet
                auto it = std::find(buffer.begin(), buffer.end(), 0xAA);
                if (it == buffer.end()) {
                    buffer.clear();
                    break;
                }
                
                // Check if we have the second header byte
                if (std::distance(it, buffer.end()) < 2 || *(it + 1) != 0xBB) {
                    buffer.erase(buffer.begin(), it + 1);
                    continue;
                }
                
                // Check if we have a complete packet
                if (std::distance(it, buffer.end()) >= 14) {
                    std::vector<uint8_t> packet_data(it, it + 14);
                    
                    // Verify checksum
                    uint8_t checksum = 0;
                    for (size_t i = 2; i < 13; ++i) {
                        checksum ^= packet_data[i];
                    }
                    
                    if (checksum == packet_data[13]) {
                        // Valid packet
                        SensorDataPacket packet = parse_uart_packet(packet_data);
                        if (data_callback_ && packet.is_valid) {
                            data_callback_(packet);
                        }
                        
                        std::cout << "📨 [UART] Received valid packet from sensor: " 
                                  << packet.sensor_id << std::endl;
                    } else {
                        std::cout << "⚠️ [UART] Invalid checksum, packet discarded" << std::endl;
                    }
                    
                    buffer.erase(buffer.begin(), it + 14);
                } else {
                    break; // Wait for more data
                }
            }
        } else if (bytes_read < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "❌ [UART] Read error: " << strerror(errno) << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "🏁 [UART] Reader thread finished" << std::endl;
}

SensorDataPacket UARTInterface::parse_uart_packet(const std::vector<uint8_t>& data) {
    SensorDataPacket packet = {};
    packet.timestamp = std::chrono::steady_clock::now();
    packet.interface_used = CommInterface::UART_INTERFACE;
    packet.is_valid = false;
    
    if (data.size() < 14) {
        return packet;
    }
    
    // Parse packet: [0xAA][0xBB][NodeID(4)][Temp(2)][Humidity(2)][Voltage(2)][Status(1)][Checksum(1)]
    
    // Extract node ID (simplified hash)
    uint32_t node_hash = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
    packet.sensor_id = "sensor_" + std::to_string(node_hash % 10000);
    packet.location = "Unknown";
    
    // Extract temperature (int16 * 100)
    int16_t temp_raw = (data[6] << 8) | data[7];
    packet.temperature_celsius = temp_raw / 100.0f;
    packet.raw_temp_adc = (temp_raw + 4000) * 4095 / 12500; // Estimate ADC value
    
    // Extract humidity (uint16 * 100)
    uint16_t hum_raw = (data[8] << 8) | data[9];
    packet.humidity_percent = hum_raw / 100.0f;
    packet.raw_humidity_adc = (hum_raw * 4095) / 10000; // Estimate ADC value
    
    // Extract voltage (uint16 * 1000)
    uint16_t volt_raw = (data[10] << 8) | data[11];
    packet.supply_voltage = volt_raw / 1000.0f;
    
    // Extract status
    packet.sensor_status = data[12];
    
    // Set default values
    packet.pressure_hpa = 0.0f; // Not available in this packet
    packet.signal_strength = 1.0f; // Wired connection
    packet.packet_sequence = 0; // Would need to track this
    packet.data_confidence = 0.95f; // High confidence for wired connection
    
    // Validate data ranges
    if (packet.temperature_celsius >= -50.0f && packet.temperature_celsius <= 100.0f &&
        packet.humidity_percent >= 0.0f && packet.humidity_percent <= 100.0f &&
        packet.supply_voltage >= 2.0f && packet.supply_voltage <= 5.0f) {
        packet.is_valid = true;
    }
    
    return packet;
}

//=============================================================================
// SPI Interface Implementation
//=============================================================================

SPIInterface::SPIInterface(const std::string& device, int speed)
    : device_(device), speed_(speed), fd_(-1), active_(false) {
    std::cout << "🔌 [SPI] Interface created for device: " << device_ 
              << " @ " << speed_ << " Hz" << std::endl;
}

SPIInterface::~SPIInterface() {
    stop();
    if (fd_ >= 0) {
        close(fd_);
    }
    std::cout << "🔌 [SPI] Interface destroyed" << std::endl;
}

bool SPIInterface::initialize() {
    std::cout << "🚀 [SPI] Initializing interface..." << std::endl;
    
    // Open SPI device
    fd_ = open(device_.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "❌ [SPI] Failed to open device: " << device_ << std::endl;
        return false;
    }
    
    // Configure SPI mode
    uint8_t mode = SPI_MODE_0;
    if (ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0) {
        std::cerr << "❌ [SPI] Failed to set SPI mode" << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Configure bits per word
    uint8_t bits = 8;
    if (ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        std::cerr << "❌ [SPI] Failed to set bits per word" << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Configure speed
    if (ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_) < 0) {
        std::cerr << "❌ [SPI] Failed to set speed" << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }
    
    std::cout << "✅ [SPI] Interface initialized successfully" << std::endl;
    return true;
}

bool SPIInterface::start() {
    if (fd_ < 0) {
        std::cerr << "❌ [SPI] Interface not initialized" << std::endl;
        return false;
    }
    
    if (active_.load()) {
        std::cout << "⚠️ [SPI] Interface already running" << std::endl;
        return true;
    }
    
    active_ = true;
    polling_thread_ = std::thread(&SPIInterface::polling_loop, this);
    
    std::cout << "🚀 [SPI] Interface started" << std::endl;
    return true;
}

void SPIInterface::stop() {
    if (!active_.load()) {
        return;
    }
    
    std::cout << "🛑 [SPI] Stopping interface..." << std::endl;
    active_ = false;
    
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
    
    std::cout << "✅ [SPI] Interface stopped" << std::endl;
}

void SPIInterface::set_data_callback(std::function<void(const SensorDataPacket&)> callback) {
    data_callback_ = callback;
}

void SPIInterface::polling_loop() {
    std::cout << "🔄 [SPI] Polling thread started" << std::endl;
    
    while (active_.load()) {
        // SPI is typically request-response, so we poll for data
        std::vector<uint8_t> tx_buffer(14, 0x00); // Send zeros to request data
        std::vector<uint8_t> rx_buffer(14, 0x00);
        
        struct spi_ioc_transfer transfer = {};
        transfer.tx_buf = reinterpret_cast<uintptr_t>(tx_buffer.data());
        transfer.rx_buf = reinterpret_cast<uintptr_t>(rx_buffer.data());
        transfer.len = 14;
        transfer.speed_hz = speed_;
        transfer.bits_per_word = 8;
        
        if (ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer) >= 0) {
            // Check if we received valid data (starts with 0xAA 0xBB)
            if (rx_buffer[0] == 0xAA && rx_buffer[1] == 0xBB) {
                SensorDataPacket packet = parse_spi_packet(rx_buffer);
                if (data_callback_ && packet.is_valid) {
                    data_callback_(packet);
                    std::cout << "📨 [SPI] Received valid packet from sensor: " 
                              << packet.sensor_id << std::endl;
                }
            }
        } else {
            std::cerr << "❌ [SPI] Transfer failed: " << strerror(errno) << std::endl;
        }
        
        // Poll every 500ms to avoid overwhelming the SPI bus
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    std::cout << "🏁 [SPI] Polling thread finished" << std::endl;
}

SensorDataPacket SPIInterface::parse_spi_packet(const std::vector<uint8_t>& data) {
    SensorDataPacket packet = {};
    packet.timestamp = std::chrono::steady_clock::now();
    packet.interface_used = CommInterface::SPI_INTERFACE;
    packet.is_valid = false;
    
    if (data.size() < 14) {
        return packet;
    }
    
    // Verify checksum
    uint8_t checksum = 0;
    for (size_t i = 2; i < 13; ++i) {
        checksum ^= data[i];
    }
    
    if (checksum != data[13]) {
        return packet; // Invalid checksum
    }
    
    // Parse packet (same format as UART)
    uint32_t node_hash = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
    packet.sensor_id = "spi_sensor_" + std::to_string(node_hash % 10000);
    packet.location = "SPI_Bus";
    
    int16_t temp_raw = (data[6] << 8) | data[7];
    packet.temperature_celsius = temp_raw / 100.0f;
    packet.raw_temp_adc = (temp_raw + 4000) * 4095 / 12500;
    
    uint16_t hum_raw = (data[8] << 8) | data[9];
    packet.humidity_percent = hum_raw / 100.0f;
    packet.raw_humidity_adc = (hum_raw * 4095) / 10000;
    
    uint16_t volt_raw = (data[10] << 8) | data[11];
    packet.supply_voltage = volt_raw / 1000.0f;
    
    packet.sensor_status = data[12];
    packet.pressure_hpa = 0.0f;
    packet.signal_strength = 1.0f;
    packet.packet_sequence = 0;
    packet.data_confidence = 0.98f; // Very high confidence for SPI
    
    // Validate data ranges
    if (packet.temperature_celsius >= -50.0f && packet.temperature_celsius <= 100.0f &&
        packet.humidity_percent >= 0.0f && packet.humidity_percent <= 100.0f &&
        packet.supply_voltage >= 2.0f && packet.supply_voltage <= 5.0f) {
        packet.is_valid = true;
    }
    
    return packet;
}

//=============================================================================
// I2C Interface Implementation
//=============================================================================

I2CInterface::I2CInterface(int bus, const std::vector<int>& addresses)
    : bus_(bus), addresses_(addresses), fd_(-1), active_(false) {
    std::cout << "🔌 [I2C] Interface created for bus: " << bus_ 
              << " with " << addresses_.size() << " sensor addresses" << std::endl;
}

I2CInterface::~I2CInterface() {
    stop();
    if (fd_ >= 0) {
        close(fd_);
    }
    std::cout << "🔌 [I2C] Interface destroyed" << std::endl;
}

bool I2CInterface::initialize() {
    std::cout << "🚀 [I2C] Initializing interface..." << std::endl;
    
    // Open I2C device
    std::string device = "/dev/i2c-" + std::to_string(bus_);
    fd_ = open(device.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "❌ [I2C] Failed to open device: " << device << std::endl;
        return false;
    }
    
    std::cout << "✅ [I2C] Interface initialized successfully" << std::endl;
    return true;
}

bool I2CInterface::start() {
    if (fd_ < 0) {
        std::cerr << "❌ [I2C] Interface not initialized" << std::endl;
        return false;
    }
    
    if (active_.load()) {
        std::cout << "⚠️ [I2C] Interface already running" << std::endl;
        return true;
    }
    
    active_ = true;
    polling_thread_ = std::thread(&I2CInterface::polling_loop, this);
    
    std::cout << "🚀 [I2C] Interface started" << std::endl;
    return true;
}

void I2CInterface::stop() {
    if (!active_.load()) {
        return;
    }
    
    std::cout << "🛑 [I2C] Stopping interface..." << std::endl;
    active_ = false;
    
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
    
    std::cout << "✅ [I2C] Interface stopped" << std::endl;
}

void I2CInterface::set_data_callback(std::function<void(const SensorDataPacket&)> callback) {
    data_callback_ = callback;
}

void I2CInterface::polling_loop() {
    std::cout << "🔄 [I2C] Polling thread started" << std::endl;
    
    while (active_.load()) {
        // Poll each I2C address
        for (int address : addresses_) {
            std::vector<uint8_t> data;
            if (read_i2c_sensor(address, data)) {
                SensorDataPacket packet = parse_i2c_packet(address, data);
                if (data_callback_ && packet.is_valid) {
                    data_callback_(packet);
                    std::cout << "📨 [I2C] Received valid packet from address: 0x" 
                              << std::hex << address << std::dec << std::endl;
                }
            }
        }
        
        // Poll every 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "🏁 [I2C] Polling thread finished" << std::endl;
}

bool I2CInterface::read_i2c_sensor(int address, std::vector<uint8_t>& data) {
    // Set I2C slave address
    if (ioctl(fd_, I2C_SLAVE, address) < 0) {
        return false;
    }
    
    // Different sensors have different read methods
    // For demonstration, we'll simulate reading different sensor types
    data.clear();
    
    if (address == 0x76 || address == 0x77) {
        // BME280 sensor simulation
        data.resize(8);
        // Simulate reading temperature, humidity, and pressure registers
        // In real implementation, you'd read actual sensor registers
        data[0] = 0x80; // temp MSB
        data[1] = 0x00; // temp LSB
        data[2] = 0x80; // humidity MSB
        data[3] = 0x00; // humidity LSB
        data[4] = 0x80; // pressure MSB
        data[5] = 0x00; // pressure middle
        data[6] = 0x00; // pressure LSB
        data[7] = 0x01; // status
        
        return true;
    } else if (address == 0x44 || address == 0x45) {
        // SHT30 sensor simulation
        data.resize(6);
        data[0] = 0x66; // temp MSB
        data[1] = 0x00; // temp LSB
        data[2] = 0x5A; // temp CRC
        data[3] = 0x7F; // humidity MSB
        data[4] = 0x00; // humidity LSB
        data[5] = 0x9D; // humidity CRC
        
        return true;
    }
    
    return false; // Unknown sensor type
}

SensorDataPacket I2CInterface::parse_i2c_packet(int address, const std::vector<uint8_t>& data) {
    SensorDataPacket packet = {};
    packet.timestamp = std::chrono::steady_clock::now();
    packet.interface_used = CommInterface::I2C_INTERFACE;
    packet.is_valid = false;
    packet.sensor_id = "i2c_" + std::to_string(address);
    packet.location = "I2C_Bus_" + std::to_string(bus_);
    
    if (address == 0x76 || address == 0x77) {
        // BME280 parsing
        if (data.size() >= 8) {
            // Simplified BME280 data parsing
            uint16_t temp_raw = (data[0] << 8) | data[1];
            uint16_t hum_raw = (data[2] << 8) | data[3];
            uint32_t press_raw = (data[4] << 16) | (data[5] << 8) | data[6];
            
            // Convert to physical values (simplified)
            packet.temperature_celsius = 20.0f + (temp_raw - 32768) / 100.0f;
            packet.humidity_percent = (hum_raw * 100.0f) / 65535.0f;
            packet.pressure_hpa = 1013.25f + (press_raw - 524288) / 256.0f;
            
            packet.raw_temp_adc = temp_raw;
            packet.raw_humidity_adc = hum_raw;
            packet.sensor_status = data[7];
            packet.supply_voltage = 3.3f; // Typical for I2C sensors
            packet.signal_strength = 1.0f;
            packet.packet_sequence = 0;
            packet.data_confidence = 0.95f;
            
            packet.is_valid = true;
        }
    } else if (address == 0x44 || address == 0x45) {
        // SHT30 parsing
        if (data.size() >= 6) {
            uint16_t temp_raw = (data[0] << 8) | data[1];
            uint16_t hum_raw = (data[3] << 8) | data[4];
            
            // Convert to physical values
            packet.temperature_celsius = -45.0f + (175.0f * temp_raw / 65535.0f);
            packet.humidity_percent = (100.0f * hum_raw) / 65535.0f;
            packet.pressure_hpa = 0.0f; // SHT30 doesn't measure pressure
            
            packet.raw_temp_adc = temp_raw;
            packet.raw_humidity_adc = hum_raw;
            packet.sensor_status = 0x01; // Assume OK
            packet.supply_voltage = 3.3f;
            packet.signal_strength = 1.0f;
            packet.packet_sequence = 0;
            packet.data_confidence = 0.98f; // Very high for SHT30
            
            packet.is_valid = true;
        }
    }
    
    return packet;
}

} // namespace rpi4_gateway 