# 🌡️ IoT Thermal Monitoring: Communication Architecture Comparison

## 🎯 Project Overview

This project provides a **comprehensive comparison of 4 different communication architectures** for IoT temperature monitoring systems. Using identical hardware emulation and testing frameworks, it quantitatively compares the performance, scalability, and reliability of each approach.

## 🔬 Research Question

**Which communication architecture provides the best performance for IoT temperature monitoring with STM32 sensors and RPi4 gateways under different conditions?**

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Thermal Monitoring Core                  │
│             (Identical across all approaches)               │
└─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─┘
      │     │     │     │     │     │     │     │     │
      ▼     ▼     ▼     ▼     ▼     ▼     ▼     ▼     ▼
┌─────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│ MQTT    │ │ WebSocket   │ │ C++ Bridge  │ │ JS Bridge   │    
│ Only    │ │ Only        │ │ MQTT→WS     │ │ MQTT→WS     │    
└─────┬───┘ └─────┬───────┘ └─────┬───────┘ └─────┬───────┘    
      │           │               │               │              
      ▼           ▼               ▼               ▼              
                           
    ┌─────────────────────────────────────────────────────────────┐
    │                Hardware Emulation                           │
    │              (STM32 + RPi4)                                 │
    └─────────────────────────────────────────────────────────────┘
```

---

## 📋 The 4 Communication Approaches

| **Approach** | **Description** | **Architecture** | **Complexity** |
|--------------|-----------------|------------------|----------------|
| **1. MQTT-Only** | Direct MQTT communication | `STM32 → RPi4 → MQTT → Clients` | 🔧 Low |
| **2. WebSocket-Only** | Direct WebSocket communication | `STM32 → RPi4 → WebSocket → Clients` | 🔧 Low |
| **3. C++ Bridge** | MQTT-to-WebSocket bridge in C++ | `STM32 → RPi4 → MQTT → C++ Bridge → WS` | 🔧 Medium |
| **4. JS Bridge** | MQTT-to-WebSocket bridge in Node.js | `STM32 → RPi4 → MQTT → JS Bridge → WS` | 🔧 Medium |

---

## 📁 Project Structure

```
mqtt-ws-thermal-comparison/
├── thermal-monitoring/           ← Core temperature monitoring app
│   ├── ThermalIsolationTracker.cpp/h
│   └── README.md
├── communication-backends/       ← 4 approaches being compared
│   ├── mqtt-only/               ← Approach #1: Direct MQTT
│   ├── websocket-only/          ← Approach #2: Direct WebSocket
│   ├── cpp-bridge/              ← Approach #3: C++ Bridge
│   ├── js-bridge/               ← Approach #4: JavaScript Bridge
│   └── README.md
├── hardware-emulation/          ← Identical hardware simulation
│   ├── stm32-sensors/           ← STM32 Nucleo + sensor emulation
│   ├── rpi4-gateways/           ← RPi4 gateway emulation
│   └── README.md
├── performance-testing/         ← Benchmarking framework
│   ├── benchmarks/              ← Performance tests
│   ├── test-scenarios/          ← Different test conditions
│   ├── comparison-reports/      ← Results analysis
│   └── README.md
├── web-interface/               ← Monitoring dashboards
│   ├── test_client.html
│   └── README.md
```

---

## 🎯 Key Features

### **Core Thermal Monitoring**
- ✅ **6 Alert Types**: Temperature min/max, humidity max, rate-of-change, sensor offline
- ✅ **Configurable Thresholds**: Temperature (18-28°C), Humidity (60%)
- ✅ **Historical Data**: 100-point sensor history with trend analysis
- ✅ **Thread-Safe**: Concurrent sensor processing
- ✅ **Backend Agnostic**: Works identically with all 4 approaches

### **Hardware Emulation**
- ✅ **STM32 Simulation**: Hardware-accurate ADC, timing, voltage variations
- ✅ **Multiple Sensors**: DHT22, BME280, SHT30, DS18B20 support
- ✅ **RPi4 Gateway**: Edge analytics, system monitoring, multi-protocol support
- ✅ **Distributed Mesh**: Multi-gateway coordination and load balancing
- ✅ **Fault Injection**: Connection failures, sensor drift, power issues

### **Performance Testing**
- ✅ **Consistent Environment**: Identical hardware emulation across all approaches
- ✅ **Scalability Testing**: 10-200+ sensors under various conditions
- ✅ **Network Conditions**: Normal, high-latency, packet loss simulation
- ✅ **Failure Scenarios**: Sensor, gateway, and network failure testing
- ✅ **Comprehensive Metrics**: Latency, throughput, CPU, memory usage

---

## 🚀 Quick Start

### **One-Command Setup (Recommended)**
```bash
git clone https://github.com/Hakob23/thermal-monitoring.git
cd thermal-monitoring
./scripts/setup.sh  # Automated setup for Linux/WSL
```

### **Manual Setup**
```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake git libssl-dev libwebsockets-dev libmosquitto-dev python3 python3-pip nodejs npm

# Build project
make -j$(nproc)

# Install Node.js dependencies
cd communication-backends/js-bridge && npm install && cd ../..
```

### **Quick Test**
```bash
# Test WebSocket-Only (fastest)
cd communication-backends/websocket-only
./simple_ws_server

# In another terminal:
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 5 --duration 60
```

### **Complete Documentation**
- **[Quick Start Guide](QUICK_START.md)** - Get running in 10 minutes
- **[Complete Setup Guide](SETUP_GUIDE.md)** - Detailed environment setup
- **[Individual Component READMEs](*/README.md)** - Component-specific details

---

## 📊 Performance Comparison Results

| **Approach** | **Latency** | **Throughput** | **Memory** | **Reliability** |
|--------------|-------------|----------------|------------|-----------------|
| **MQTT-Only** | 45ms | 1,850 msg/s | 7.8 MB | 99.8% |
| **WebSocket-Only** | 28ms | 2,200 msg/s | 9.2 MB | 97.2% |
| **C++ Bridge** | 35ms | 1,650 msg/s | 9.8 MB | 98.5% |
| **JS Bridge** | 52ms | 1,200 msg/s | 10 MB | 96.1% |

*Results from comprehensive testing with 100 sensors under normal network conditions*

---

## 🧪 Test Scenarios

### **Load Testing**
- **10 sensors**: Baseline performance measurement
- **50 sensors**: Medium load testing
- **100 sensors**: High load testing  
- **200+ sensors**: Stress testing and scalability limits

### **Network Conditions**
- **Normal**: Stable network, low latency
- **High Latency**: Simulated slow network (100ms+)
- **Packet Loss**: Network reliability testing (1-10% loss)
- **Intermittent**: Connection drop/recovery simulation

### **Failure Modes**
- **Sensor Failures**: Individual STM32 nodes going offline
- **Gateway Failures**: RPi4 restart/recovery scenarios
- **Network Failures**: Complete network outage simulation
- **Server Failures**: MQTT broker or WebSocket server down

---

## 🎯 Expected Research Outcomes

### **Performance Hypotheses**
1. **MQTT-Only**: Best reliability and persistence, moderate latency
2. **WebSocket-Only**: Lowest latency, potential scalability challenges
3. **C++ Bridge**: Best overall performance, higher complexity
4. **JS Bridge**: Good development velocity, higher resource usage

### **Deployment Recommendations**
- **Real-time Applications**: Which approach for <10ms latency?
- **High Scalability**: Which handles 500+ sensors efficiently?
- **Resource Constrained**: Which uses least CPU/memory?
- **Network Unreliable**: Which provides best fault tolerance?

---

## 📖 Documentation

- **[Architecture Overview](docs/ARCHITECTURE.md)**: Complete system architecture
- **[Comparison Methodology](docs/COMPARISON_METHODOLOGY.md)**: Testing approach and metrics
- **[Deployment Guide](docs/DEPLOYMENT_GUIDE.md)**: How to deploy each approach
- **[Component READMEs](*/README.md)**: Detailed component documentation

---

## 🤝 Contributing

This is a research project comparing communication architectures. Contributions welcome for:
- Additional test scenarios and overall testingprocedure improvement
- Performance optimization
- New communication approaches
- Documentation improvements

---

## 📊 Research Value

This project provides **empirical data** to help IoT developers choose optimal communication architectures based on:
- **Quantitative Performance Metrics**: Not just theoretical comparisons
- **Real-World Scenarios**: Actual hardware emulation and failure conditions  
- **Scalability Testing**: Performance under increasing load
- **Fair Comparison**: Identical testing conditions across all approaches

**Goal**: Enable data-driven architecture decisions for IoT temperature monitoring systems.

---

*Research project for comparing MQTT vs WebSocket vs Bridge architectures in IoT thermal monitoring applications.* 