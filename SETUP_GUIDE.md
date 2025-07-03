# 🚀 IoT Thermal Monitoring: Complete Setup Guide

This guide will walk you through setting up the entire IoT thermal monitoring system with all four communication architectures for performance comparison.

## 📋 Prerequisites

### System Requirements
- **Operating System**: Linux (Ubuntu 20.04+ recommended) or WSL2 on Windows
- **RAM**: Minimum 4GB, Recommended 8GB+
- **Storage**: 2GB free space
- **Network**: Internet connection for dependencies

### Required Software
- **GCC/G++**: Version 9.0 or higher
- **Make**: Build system
- **CMake**: Version 3.16 or higher
- **Node.js**: Version 16.0 or higher (for JavaScript bridge)
- **Python**: Version 3.8 or higher (for testing scripts)
- **Git**: Version control

## 🔧 Environment Setup

### 1. Install System Dependencies

#### Ubuntu/Debian:
```bash
sudo apt update
sudo apt install -y build-essential cmake git curl wget
sudo apt install -y libssl-dev libwebsockets-dev libmosquitto-dev
sudo apt install -y python3 python3-pip nodejs npm
```

#### CentOS/RHEL:
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake git curl wget
sudo yum install -y openssl-devel libwebsockets-devel mosquitto-devel
sudo yum install -y python3 python3-pip nodejs npm
```

#### WSL2 (Windows):
```bash
# Install WSL2 if not already installed
wsl --install -d Ubuntu

# Then follow Ubuntu instructions above
```

### 2. Install Node.js Dependencies

```bash
# Install Node.js dependencies for JavaScript bridge
cd communication-backends/js-bridge
npm install
cd ../..
```

### 3. Install Python Dependencies

```bash
# Install Python testing utilities
pip3 install pytest requests websocket-client paho-mqtt
```

## 🏗️ Building the Project

### 1. Build All Components

```bash
# Clone the repository (if not already done)
git clone https://github.com/Hakob23/thermal-monitoring.git
cd thermal-monitoring

# Build all C++ components
make -j$(nproc)
```

### 2. Build Individual Components

#### Communication Backends:
```bash
# MQTT-Only
cd communication-backends/mqtt-only
make clean && make
cd ../..

# WebSocket-Only
cd communication-backends/websocket-only
make clean && make
cd ../..

# C++ Bridge
cd communication-backends/cpp-bridge
make clean && make
cd ../..

# JavaScript Bridge
cd communication-backends/js-bridge
npm install
cd ../..
```

#### Hardware Emulation:
```bash
# STM32 Sensor Simulators
cd hardware-emulation/stm32-sensors
make clean && make
cd ../..

# RPi4 Gateway
cd hardware-emulation/rpi4-gateways
make clean && make
cd ../..
```

#### Performance Testing:
```bash
# Integration Tests
cd performance-testing/benchmarks
make clean && make
cd ../..
```

## 🚀 Running the Application

### 1. Quick Start - Single Architecture Test

#### Test MQTT-Only Architecture:
```bash
# Terminal 1: Start MQTT broker
mosquitto -p 1883

# Terminal 2: Start MQTT client
cd communication-backends/mqtt-only
./simple_mqtt_client

# Terminal 3: Start STM32 simulator
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 5 --duration 60
```

#### Test WebSocket-Only Architecture:
```bash
# Terminal 1: Start WebSocket server
cd communication-backends/websocket-only
./simple_ws_server

# Terminal 2: Start STM32 simulator
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 5 --duration 60
```

### 2. Complete System Test

#### Step 1: Start All Services
```bash
# Terminal 1: MQTT Broker
mosquitto -p 1883

# Terminal 2: C++ Bridge
cd communication-backends/cpp-bridge
./bin/mqtt_ws_bridge

# Terminal 3: JavaScript Bridge
cd communication-backends/js-bridge
node bin/mqttwsBridge.js

# Terminal 4: WebSocket Server
cd communication-backends/websocket-only
./simple_ws_server
```

#### Step 2: Start Hardware Emulation
```bash
# Terminal 5: STM32 Sensors
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 10 --duration 300

# Terminal 6: RPi4 Gateway
cd hardware-emulation/rpi4-gateways
./bin/test_rpi4_gateway --demo
```

#### Step 3: Run Performance Tests
```bash
# Terminal 7: Integration Tests
cd performance-testing/benchmarks
./bin/integration_tests --suite All --verbose
```

### 3. Web Interface

#### Start Web Monitoring Dashboard:
```bash
# Terminal 8: Web Interface
cd web-interface
python3 -m http.server 8080
```

Then open your browser to: `http://localhost:8080/test_client.html`

## 📊 Performance Testing

### 1. Run All Performance Benchmarks

```bash
cd performance-testing/benchmarks

# Basic performance test
./bin/integration_tests --test PerformanceBenchmark --verbose

# Stress testing
./bin/integration_tests --test StressLoad --verbose

# Fault tolerance testing
./bin/integration_tests --test FaultTolerance --verbose

# Complete test suite
./bin/integration_tests --suite All --verbose --output results.json
```

### 2. Compare Architectures

```bash
# Test MQTT-Only
./bin/integration_tests --test MQTTOnly --verbose --output mqtt_results.json

# Test WebSocket-Only
./bin/integration_tests --test WebSocketOnly --verbose --output ws_results.json

# Test C++ Bridge
./bin/integration_tests --test CppBridge --verbose --output cpp_results.json

# Test JavaScript Bridge
./bin/integration_tests --test JsBridge --verbose --output js_results.json
```

### 3. Generate Performance Reports

```bash
# Generate comparison report
python3 generate_report.py mqtt_results.json ws_results.json cpp_results.json js_results.json
```

## 🔧 Configuration

### 1. MQTT Configuration

Edit `communication-backends/mqtt-only/config.json`:
```json
{
  "broker": "localhost",
  "port": 1883,
  "client_id": "thermal_monitor",
  "topics": {
    "sensor_data": "sensors/+/data",
    "alerts": "alerts/thermal"
  }
}
```

### 2. WebSocket Configuration

Edit `communication-backends/websocket-only/config.json`:
```json
{
  "port": 8080,
  "max_clients": 100,
  "heartbeat_interval": 30
}
```

### 3. Bridge Configuration

Edit `communication-backends/cpp-bridge/config.json`:
```json
{
  "mqtt": {
    "broker": "localhost",
    "port": 1883
  },
  "websocket": {
    "port": 8081,
    "max_clients": 50
  }
}
```

## 🐛 Troubleshooting

### Common Issues

#### 1. Build Errors
```bash
# Clean and rebuild
make clean
make -j$(nproc)

# Check dependencies
ldd ./bin/integration_tests
```

#### 2. Port Conflicts
```bash
# Check what's using a port
sudo netstat -tulpn | grep :1883

# Kill process using port
sudo kill -9 <PID>
```

#### 3. Permission Issues
```bash
# Fix permissions
chmod +x bin/*
chmod +x scripts/*
```

#### 4. Memory Issues
```bash
# Check available memory
free -h

# Increase swap if needed
sudo fallocate -l 2G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### Debug Mode

```bash
# Enable debug logging
export DEBUG=1
export LOG_LEVEL=DEBUG

# Run with verbose output
./bin/integration_tests --verbose --debug
```

## 📈 Monitoring and Logs

### 1. View Real-time Logs

```bash
# Follow all logs
tail -f logs/*.log

# Follow specific component
tail -f logs/mqtt_bridge.log
```

### 2. System Monitoring

```bash
# Monitor system resources
htop

# Monitor network connections
netstat -tulpn | grep :8080
```

### 3. Performance Monitoring

```bash
# Monitor memory usage
watch -n 1 'ps aux | grep integration_tests'

# Monitor CPU usage
top -p $(pgrep integration_tests)
```

## 🧪 Testing Scenarios

### 1. Basic Functionality Test

```bash
# Test single sensor
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 1 --duration 30
```

### 2. Load Testing

```bash
# Test with 50 sensors
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 50 --duration 120
```

### 3. Stress Testing

```bash
# Test with 200 sensors
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 200 --duration 300
```

### 4. Fault Tolerance Testing

```bash
# Test network failures
cd performance-testing/benchmarks
./bin/integration_tests --test FaultTolerance --verbose
```

## 📊 Results Analysis

### 1. View Test Results

```bash
# View latest results
cat results/latest_test_results.json

# Compare architectures
python3 analyze_results.py results/
```

### 2. Generate Performance Charts

```bash
# Generate performance comparison charts
python3 generate_charts.py results/
```

### 3. Export Results

```bash
# Export to CSV
python3 export_results.py results/ --format csv

# Export to Excel
python3 export_results.py results/ --format excel
```

## 🔄 Continuous Integration

### 1. Automated Testing

```bash
# Run CI pipeline
./scripts/ci_pipeline.sh

# Run specific test suite
./scripts/run_tests.sh --suite performance
```

### 2. Code Quality Checks

```bash
# Run static analysis
make analyze

# Run memory leak detection
make test-memory

# Run coverage analysis
make coverage
```

## 📚 Additional Resources

- **Documentation**: See individual component README files
- **API Reference**: Check header files in `include/` directories
- **Examples**: See `examples/` directories in each component
- **Troubleshooting**: Check `logs/` directory for error details

## 🆘 Getting Help

If you encounter issues:

1. Check the troubleshooting section above
2. Review the logs in `logs/` directory
3. Check the individual component README files
4. Open an issue on GitHub with:
   - Your system information
   - Error messages
   - Steps to reproduce
   - Log files

---

**Happy Testing! 🚀**

This setup guide will get you running the complete IoT thermal monitoring system with all four communication architectures for comprehensive performance comparison. 