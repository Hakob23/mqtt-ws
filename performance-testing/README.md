# 🚀 MQTT Performance Testing Framework

This framework provides comprehensive performance testing for the MQTT-only approach in the IoT thermal monitoring system. It measures throughput, latency, CPU usage, and memory consumption with configurable sensor counts and test durations.

## 🎯 Overview

The performance testing framework consists of:

- **MQTT Performance Test** (`mqtt_performance_test.cpp`) - C++ application that simulates sensors and measures MQTT performance
- **System Monitor** (`system_monitor.py`) - Python script that monitors CPU and memory usage
- **Test Runner** (`run_mqtt_test.sh`) - Bash script that orchestrates the complete testing process
- **Makefile** - Build system for compiling and running tests

## 📊 What Gets Measured

### **Performance Metrics**
- **Throughput**: Messages per second processed
- **Latency**: End-to-end message delivery time
- **Message Count**: Total messages sent and received
- **Alert Generation**: Number of thermal alerts triggered

### **System Resources**
- **CPU Usage**: System-wide and process-specific CPU utilization
- **Memory Usage**: System-wide and process-specific memory consumption
- **Network I/O**: Bytes sent/received and packet counts
- **Disk I/O**: Read/write operations and data transfer

## 🛠️ Prerequisites

### **Required Packages**
```bash
# Install system dependencies
sudo apt update
sudo apt install -y build-essential cmake git
sudo apt install -y mosquitto mosquitto-clients libmosquitto-dev
sudo apt install -y libjsoncpp-dev python3 python3-pip

# Install Python dependencies
pip3 install psutil
```

### **MQTT Broker**
Ensure the Mosquitto MQTT broker is running:
```bash
# Check if broker is running
pgrep mosquitto

# Start broker if not running
sudo systemctl start mosquitto
sudo systemctl enable mosquitto
```

## 🚀 Quick Start

### **1. Run Default Test (10 sensors, 60 seconds)**
```bash
cd performance-testing
./run_mqtt_test.sh
```

### **2. Run Custom Test**
```bash
# 20 sensors, 120 seconds, 50ms interval
./run_mqtt_test.sh 20 120 50

# 5 sensors, 30 seconds, 200ms interval
./run_mqtt_test.sh 5 30 200
```

### **3. Run Individual Components**

#### **Build and Run MQTT Test Only**
```bash
make clean && make all
./mqtt_performance_test 10 60 100
```

#### **Run System Monitor Only**
```bash
python3 system_monitor.py --interval 0.5 --output system_stats.json
```

## 📁 Test Results

Each test run creates a timestamped output directory containing:

```
test_results_20241201_143022/
├── mqtt_test.log          # MQTT performance test output
├── system_stats.json      # Detailed system monitoring data
└── test_report.md         # Comprehensive test report
```

### **Sample Test Report**
```markdown
# MQTT Performance Test Report

**Test Date:** Sat Dec 1 14:30:22 UTC 2024
**Configuration:**
- Sensors: 10
- Duration: 60s
- Message Interval: 100ms

## Test Results

### MQTT Performance
```
============================================================
📊 MQTT PERFORMANCE TEST RESULTS
============================================================
Test Configuration:
   Sensors: 10
   Duration: 60.00s
   Message Interval: 100ms

Performance Metrics:
   Messages Sent: 6000
   Messages Received: 6000
   Alerts Generated: 15
   Throughput: 100.00 msg/sec

System Resources:
   Average CPU Usage: 8.5%
   Average Memory Usage: 45.2 MB
   Average Latency: 2.3 ms
```

### System Resource Usage
- **Monitoring Duration:** 60.0s
- **Samples Collected:** 120
- **CPU Usage:** Avg 8.5%, Max 15.2%
- **Memory Usage:** Avg 45.2MB, Max 52.1MB
- **Process CPU:** Avg 3.2%, Max 8.1%
- **Process Memory:** Avg 12.8MB, Max 15.3MB
```

## 🔧 Configuration Options

### **Test Parameters**
- **Sensors**: Number of simulated sensors (1-100+)
- **Duration**: Test duration in seconds (10-3600)
- **Interval**: Message interval in milliseconds (10-1000)

### **System Monitor Options**
```bash
python3 system_monitor.py --help
```

Options:
- `--output`: Output file for statistics
- `--interval`: Monitoring interval in seconds
- `--process`: Process name to monitor

### **Makefile Targets**
```bash
make help                    # Show all available targets
make test-10-sensors        # Run with 10 sensors (60s)
make test-quick             # Quick test (30s)
make test-stress            # Stress test (120s, high frequency)
make test-custom            # Custom parameters
make test-suite             # Run complete test suite
```

## 📈 Performance Analysis

### **Expected Results for 10 Sensors**

| **Metric** | **Expected Range** | **Good Performance** |
|------------|-------------------|---------------------|
| **Throughput** | 50-150 msg/sec | >100 msg/sec |
| **Latency** | 1-10 ms | <5 ms |
| **CPU Usage** | 2-15% | <10% |
| **Memory Usage** | 10-50 MB | <30 MB |
| **Success Rate** | 95-100% | >99% |

### **Scaling Characteristics**
- **10 sensors**: Baseline performance measurement
- **50 sensors**: Medium load testing
- **100 sensors**: High load testing
- **200+ sensors**: Stress testing and scalability limits

## 🧪 Test Scenarios

### **1. Baseline Test (Recommended)**
```bash
./run_mqtt_test.sh 10 60 100
```
- 10 sensors, 60 seconds, 100ms interval
- Good for initial performance assessment

### **2. Quick Validation**
```bash
./run_mqtt_test.sh 5 30 200
```
- 5 sensors, 30 seconds, 200ms interval
- Fast validation of system functionality

### **3. Stress Test**
```bash
./run_mqtt_test.sh 20 120 50
```
- 20 sensors, 120 seconds, 50ms interval
- High-frequency testing for scalability

### **4. Long-term Stability**
```bash
./run_mqtt_test.sh 10 600 100
```
- 10 sensors, 10 minutes, 100ms interval
- Extended testing for stability assessment

## 🔍 Troubleshooting

### **Common Issues**

#### **MQTT Broker Not Running**
```bash
# Check broker status
sudo systemctl status mosquitto

# Start broker
sudo systemctl start mosquitto

# Check if port 1883 is open
netstat -tlnp | grep 1883
```

#### **Build Errors**
```bash
# Install missing dependencies
sudo apt install -y build-essential libmosquitto-dev libjsoncpp-dev

# Clean and rebuild
make clean && make all
```

#### **Python Dependencies**
```bash
# Install psutil
pip3 install psutil

# Or install system package
sudo apt install -y python3-psutil
```

#### **Permission Issues**
```bash
# Make script executable
chmod +x run_mqtt_test.sh

# Check file permissions
ls -la *.sh *.py
```

### **Performance Issues**

#### **High CPU Usage**
- Reduce sensor count or increase message interval
- Check for other processes consuming CPU
- Monitor system load with `htop`

#### **High Memory Usage**
- Check for memory leaks in the application
- Monitor with `free -h` and `ps aux`
- Consider reducing test duration

#### **Network Issues**
- Check MQTT broker configuration
- Monitor network with `iftop` or `nethogs`
- Verify firewall settings

## 📊 Data Analysis

### **JSON Output Format**
The system monitor produces detailed JSON data:
```json
{
  "summary": {
    "monitoring_duration_seconds": 60.0,
    "samples_collected": 120,
    "cpu_summary": {
      "average": 8.5,
      "maximum": 15.2,
      "minimum": 2.1
    },
    "memory_summary": {
      "average_mb": 45.2,
      "maximum_mb": 52.1,
      "minimum_mb": 42.8
    }
  },
  "detailed_stats": [
    {
      "timestamp": "2024-12-01T14:30:22.123456",
      "cpu_percent": 8.5,
      "memory_mb": 45.2,
      "process_cpu_percent": 3.2,
      "process_memory_mb": 12.8
    }
  ]
}
```

### **Custom Analysis Scripts**
Create custom analysis scripts using the JSON data:
```python
import json

with open('system_stats.json', 'r') as f:
    data = json.load(f)

summary = data['summary']
print(f"Average CPU: {summary['cpu_summary']['average']:.1f}%")
print(f"Peak Memory: {summary['memory_summary']['maximum_mb']:.1f}MB")
```

## 🤝 Contributing

To contribute to the performance testing framework:

1. **Add new test scenarios** in `run_mqtt_test.sh`
2. **Enhance system monitoring** in `system_monitor.py`
3. **Improve performance metrics** in `mqtt_performance_test.cpp`
4. **Update documentation** and examples

## 📚 Related Documentation

- **[Main Project README](../README.md)** - Project overview and architecture
- **[MQTT-Only Backend](../communication-backends/mqtt-only/README.md)** - MQTT implementation details
- **[Thermal Monitoring Core](../thermal-monitoring/README.md)** - Core monitoring functionality
- **[Hardware Emulation](../hardware-emulation/README.md)** - Hardware simulation details

---

*Performance testing framework for MQTT-only IoT thermal monitoring system.* 