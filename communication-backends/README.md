# 🌐 Communication Backends - 4 Approach Comparison

## Overview
This directory contains **4 different communication approaches** for the thermal monitoring system. Each approach provides the same functionality but with different architectural characteristics, performance profiles, and complexity levels.

## 🎯 Research Question
**Which communication architecture provides the best performance for IoT temperature monitoring under different conditions?**

---

## 📋 The 4 Approaches

### **1. MQTT-Only (`mqtt-only/`)**
**Direct MQTT communication without intermediary bridges**

**Architecture:**
```
STM32 Sensors → RPi4 Gateway → MQTT Broker → Thermal Monitor → MQTT Clients
```

**Characteristics:**
- ✅ **Simple**: Direct MQTT pub/sub model
- ✅ **Lightweight**: No additional bridge overhead
- ✅ **Reliable**: Built-in MQTT QoS and persistence
- ❓ **Scalability**: How does it handle 100+ sensors?
- ❓ **Latency**: Direct MQTT message latency

### **2. WebSocket-Only (`websocket-only/`)**
**Direct WebSocket communication without MQTT**

**Architecture:**
```
STM32 Sensors → RPi4 Gateway → WebSocket Server → Thermal Monitor → WebSocket Clients
```

**Characteristics:**
- ✅ **Real-time**: Low-latency bidirectional communication
- ✅ **Web-friendly**: Direct browser integration
- ✅ **Lightweight**: No message broker overhead
- ❓ **Reliability**: How does it handle connection drops?
- ❓ **Scalability**: Connection limit for WebSocket server

### **3. C++ Bridge (`cpp-bridge/`)**
**MQTT-to-WebSocket bridge implemented in C++**

**Architecture:**
```
STM32 Sensors → RPi4 Gateway → MQTT Broker → C++ Bridge → WebSocket → Clients
```

**Characteristics:**
- ✅ **Hybrid**: Combines MQTT reliability with WebSocket real-time
- ✅ **Performance**: Optimized C++ implementation
- ✅ **Flexibility**: Protocol translation capabilities
- ❓ **Complexity**: Additional bridge component to maintain
- ❓ **Resource Usage**: CPU/memory overhead of bridge

### **4. JavaScript Bridge (`js-bridge/`)**
**MQTT-to-WebSocket bridge implemented in Node.js**

**Architecture:**
```
STM32 Sensors → RPi4 Gateway → MQTT Broker → Node.js Bridge → WebSocket → Clients
```

**Characteristics:**
- ✅ **Rapid Development**: JavaScript flexibility and ecosystem
- ✅ **Web Integration**: Same language as web clients
- ✅ **JSON Processing**: Native JSON handling
- ❓ **Performance**: Node.js vs C++ performance comparison
- ❓ **Memory Usage**: JavaScript runtime overhead

---

## 📊 Performance Comparison Matrix

| **Approach** | **Latency** | **Throughput** | **CPU Usage** | **Memory** | **Complexity** | **Reliability** |
|--------------|-------------|----------------|---------------|------------|----------------|-----------------|
| MQTT-Only | ⏱️ TBD | 📈 TBD | 🔥 TBD | 💾 TBD | 🔧 Low | 🛡️ TBD |
| WebSocket-Only | ⏱️ TBD | 📈 TBD | 🔥 TBD | 💾 TBD | 🔧 Low | 🛡️ TBD |
| C++ Bridge | ⏱️ TBD | 📈 TBD | 🔥 TBD | 💾 TBD | 🔧 Medium | 🛡️ TBD |
| JS Bridge | ⏱️ TBD | 📈 TBD | 🔥 TBD | 💾 TBD | 🔧 Medium | 🛡️ TBD |

*(Performance testing will fill in these metrics)*

---

## 🧪 Test Scenarios

Each approach is tested under identical conditions:

### **Load Testing**
- **10 sensors**: Baseline performance
- **50 sensors**: Medium load testing  
- **100 sensors**: High load testing
- **200+ sensors**: Stress testing

### **Network Conditions**
- **Normal**: Stable network, low latency
- **High Latency**: Simulated slow network
- **Packet Loss**: Network reliability testing
- **Intermittent**: Connection drop/recovery

### **Failure Modes**
- **Sensor Failures**: Individual sensor malfunctions
- **Gateway Failures**: RPi4 restart/recovery
- **Network Failures**: Complete network outage
- **Server Failures**: MQTT broker or WebSocket server down

---

## 🎯 Expected Outcomes

### **Hypotheses to Test:**
1. **MQTT-Only**: Best reliability, moderate performance
2. **WebSocket-Only**: Lowest latency, potential scalability issues  
3. **C++ Bridge**: Best performance, highest complexity
4. **JS Bridge**: Good development velocity, higher resource usage

### **Key Metrics:**
- **Message Latency**: End-to-end message timing
- **Throughput**: Messages per second capacity
- **CPU Usage**: Processor utilization under load
- **Memory Usage**: RAM consumption patterns
- **Failure Recovery**: Time to recover from failures
- **Scalability Limit**: Maximum sensors before degradation

---

## 📖 Directory Structure

```
communication-backends/
├── mqtt-only/           ← Approach #1: Direct MQTT
│   ├── mqtt_only_bridge.cpp/h
│   ├── test_mqtt_only.cpp
│   ├── Makefile
│   └── README.md
├── websocket-only/      ← Approach #2: Direct WebSocket  
│   ├── ws_only_bridge.cpp/h
│   ├── test_ws_only.cpp
│   ├── Makefile
│   └── README.md
├── cpp-bridge/          ← Approach #3: C++ Bridge
│   ├── mqtt_ws_bridge.cpp/h
│   ├── test_thermal_system.cpp
│   ├── Makefile
│   └── README.md
└── js-bridge/           ← Approach #4: JavaScript Bridge
    ├── mqtt-ws-bridge.js
    ├── test-js-bridge.js
    ├── package.json
    └── README.md
```

---

## 🚀 Next Steps

1. **Individual Testing**: Test each approach separately
2. **Performance Benchmarking**: Run identical test suites
3. **Comparison Analysis**: Quantitative performance comparison
4. **Recommendation Engine**: Which approach for which scenario

**Goal**: Provide empirical data to help IoT developers choose the optimal communication architecture for their specific requirements. 