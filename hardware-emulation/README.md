# 🔧 Hardware Emulation

## Overview
This directory contains **hardware-accurate emulation** of STM32 sensor nodes and RPi4 gateways. The emulation provides **identical hardware behavior** across all 4 communication approaches, ensuring fair performance comparisons.

## 🎯 Why Hardware Emulation?

### **Consistent Testing Environment**
- ✅ **Identical sensor behavior** across all 4 approaches
- ✅ **Reproducible test conditions** for fair comparison
- ✅ **No hardware dependencies** for development and CI
- ✅ **Controlled failure injection** for reliability testing

### **Realistic Behavior**
- ✅ **Hardware-level accuracy**: ADC conversion, timing, voltage variations
- ✅ **Multiple sensor types**: DHT22, DS18B20, BME280, SHT30
- ✅ **Communication protocols**: UART, SPI, I2C simulation
- ✅ **Environmental patterns**: Indoor, outdoor, HVAC, industrial

---

## 📁 Directory Structure

```
hardware-emulation/
├── stm32-sensors/         ← STM32 Nucleo + sensor emulation
│   ├── STM32_SensorNode.cpp/h
│   ├── sensor-types/      ← DHT22, BME280, SHT30, etc.
│   ├── communication/     ← UART, SPI, I2C protocols
│   └── test_stm32_simulators
├── rpi4-gateways/         ← RPi4 gateway emulation
│   ├── RPi4_Gateway.cpp/h
│   ├── data-processing/   ← Edge analytics simulation
│   ├── system-monitoring/ ← CPU, memory, network simulation
│   └── test_rpi4_gateway
└── distributed-mesh/      ← Multi-gateway coordination
    ├── gateway-mesh/      ← Inter-gateway communication
    ├── load-balancing/    ← Dynamic load distribution
    └── fault-tolerance/   ← Failure simulation
```

---

## 🤖 STM32 Sensor Emulation (`stm32-sensors/`)

### **Hardware Simulation**
```cpp
// Realistic STM32 sensor node simulation
class STM32_SensorNode {
    // Hardware-accurate ADC conversion
    uint16_t adc_temp = (temperature + 40.0f) * 4095.0f / 125.0f;
    uint16_t adc_humidity = humidity * 4095.0f / 100.0f;
    
    // Supply voltage variation (3.3V ±0.1V)
    float supply_voltage = 3.3f + noise(-0.1f, 0.1f);
    
    // Binary packet generation (real STM32 format)
    uint8_t packet[14] = {0xAA, 0xBB, ...};
};
```

### **Supported Sensor Types**
| **Sensor** | **Measurements** | **Interface** | **Accuracy** |
|------------|------------------|---------------|--------------|
| **DHT22** | Temperature, Humidity | Digital | ±0.5°C, ±3% RH |
| **BME280** | Temp, Humidity, Pressure | I2C/SPI | ±1°C, ±3% RH, ±1hPa |
| **SHT30** | Temperature, Humidity | I2C | ±0.3°C, ±2% RH |
| **DS18B20** | Temperature | 1-Wire | ±0.5°C |

### **Environmental Patterns**
- **Indoor Stable**: 20-25°C, 40-60% humidity, minimal variation
- **Outdoor**: 15-35°C daily cycle, weather-dependent humidity
- **HVAC Controlled**: Regular heating/cooling cycles
- **Industrial**: Higher temperatures, controlled environment
- **Greenhouse**: High humidity, temperature control
- **Server Room**: Cooling systems, low humidity

### **Fault Injection**
```cpp
// Realistic fault simulation
enum class FaultType {
    CONNECTION_LOST,     // Intermittent communication
    SENSOR_DRIFT,        // Gradual calibration drift
    POWER_FLUCTUATION,   // Supply voltage variations
    OVERHEATING,         // Sensor overtemperature
    CALIBRATION_ERROR    // Systematic measurement error
};
```

---

## 🏠 RPi4 Gateway Emulation (`rpi4-gateways/`)

### **Gateway Processing Simulation**
```cpp
// Realistic RPi4 gateway behavior
class RPi4_Gateway {
    // Multi-interface communication
    std::vector<CommInterface> interfaces = {UART, SPI, I2C};
    
    // Edge analytics simulation
    EdgeAnalytics analytics;
    
    // System resource monitoring
    SystemMonitor monitor;  // CPU, memory, network
    
    // Data processing pipeline
    DataProcessor processor(worker_threads=4);
};
```

### **Processing Capabilities**
- **Multi-protocol Support**: UART, SPI, I2C sensor interfaces
- **Edge Analytics**: Real-time trend analysis and pattern detection
- **Data Aggregation**: Intelligent batching and summarization
- **Local Storage**: CSV data persistence and log management
- **System Monitoring**: CPU, memory, disk, network tracking

### **Resource Simulation**
| **Resource** | **Simulation** | **Realistic Limits** |
|--------------|----------------|----------------------|
| **CPU** | Load-based utilization | 4-core ARM Cortex-A72 |
| **Memory** | Process-based usage | 4GB RAM |
| **Storage** | I/O rate simulation | microSD card performance |
| **Network** | Bandwidth/latency | Ethernet + WiFi |

---

## 🌐 Distributed Mesh (`distributed-mesh/`)

### **Multi-Gateway Coordination**
```cpp
// Gateway mesh network simulation
class GatewayMeshNetwork {
    // Hierarchical topology
    std::map<std::string, GatewayNode> gateways;
    
    // Inter-gateway communication
    MessageRouter router;
    
    // Load balancing
    LoadBalancer balancer;
    
    // Fault tolerance
    FailoverManager failover;
};
```

### **Network Topology**
```
Master Controller (Data Center)
├── Regional Hub 1 (Region North)
│   ├── Area Coordinator 1 (Building A)
│   │   ├── Edge Gateway 1 (Floor 1)
│   │   └── Edge Gateway 2 (Floor 2)
│   └── Area Coordinator 2 (Building B)
│       ├── Edge Gateway 3 (Floor 1)
│       └── Edge Gateway 4 (Floor 2)
└── Regional Hub 2 (Region South)
    └── ... (similar structure)
```

---

## 🔄 Integration with Communication Backends

### **Backend-Agnostic Design**
The hardware emulation is designed to work identically with all 4 approaches:

```cpp
// Same hardware emulation, different backends
void STM32_SensorNode::send_data(const SensorReading& reading) {
    switch (communication_backend) {
        case MQTT_ONLY:
            mqtt_client.publish("sensors/data", reading.to_json());
            break;
        case WEBSOCKET_ONLY:
            ws_client.send(reading.to_json());
            break;
        case CPP_BRIDGE:
            mqtt_client.publish("bridge/input", reading.to_json());
            break;
        case JS_BRIDGE:
            mqtt_client.publish("js-bridge/input", reading.to_json());
            break;
    }
}
```

### **Consistent Data Format**
```json
{
    "sensor_id": "STM32_001",
    "timestamp": 1234567890,
    "temperature": 23.5,
    "humidity": 45.2,
    "pressure": 1013.25,
    "supply_voltage": 3.28,
    "location": "Living Room"
}
```

---

## 🧪 Testing Scenarios

### **Load Testing**
- **10 sensors**: 1 RPi4 gateway, baseline performance
- **50 sensors**: 2 RPi4 gateways, medium load
- **100 sensors**: 4 RPi4 gateways, high load
- **200+ sensors**: 8+ RPi4 gateways, stress testing

### **Failure Simulation**
- **Sensor Failures**: Individual STM32 nodes going offline
- **Gateway Failures**: RPi4 restart/recovery scenarios
- **Network Failures**: Communication interruptions
- **Power Issues**: Voltage fluctuations and brownouts

### **Environmental Conditions**
- **Normal Operations**: Stable temperature/humidity
- **Extreme Conditions**: Temperature spikes, humidity surges
- **Daily Cycles**: Realistic day/night temperature variations
- **Seasonal Changes**: Long-term environmental trends

---

## 📊 Performance Metrics

The hardware emulation provides consistent metrics across all approaches:

- **Message Generation Rate**: Sensors per second
- **Processing Latency**: Gateway processing time
- **Resource Utilization**: CPU, memory, network usage
- **Failure Recovery Time**: Time to detect and recover from failures
- **Data Accuracy**: Sensor measurement precision
- **Communication Reliability**: Message success rate

**Goal**: Ensure all 4 communication approaches are tested under identical hardware conditions for fair performance comparison. 