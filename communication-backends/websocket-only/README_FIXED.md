# WebSocket-Only Thermal Monitoring

## 🎯 Architecture Overview

This implements a **direct WebSocket server** approach for thermal monitoring - **NO BRIDGE REQUIRED**.

```
 Thermal App               WebSocket
(WebSocket Server)   ↔    Clients
                    
✅ Direct WebSocket communication
✅ No MQTT broker needed
✅ No bridge complexity
✅ Real-time sensor data streaming
✅ Alert broadcasting to all clients
```

## 🔧 Key Components

- **SimpleWebSocketServer**: Direct WebSocket server implementation
- **ThermalIsolationTracker**: Core thermal monitoring (shared across all approaches)
- **Client Management**: Handle multiple WebSocket connections
- **Performance Metrics**: Track throughput and latency

## 🚀 Quick Start

### Prerequisites
```bash
# Install dependencies
make install-deps
```

### Build & Run
```bash
# Build the server
make

# Run test simulation
make test

# Or run server manually
make run
```

## 📊 Performance Testing

The WebSocket server will:
1. Start listening on port 8080
2. Simulate 5 sensors with realistic data
3. Process thermal monitoring alerts
4. Broadcast to all connected WebSocket clients
5. Print performance statistics

### Expected Output:
```
🚀 WebSocket-Only Thermal Monitoring Server
===========================================
✅ WebSocket Thermal Server started on port 8080
🔄 Simulating sensor data for 20 seconds...
✅ Client connected: client_1 (Total: 1)
📊 [sensor_1] Temp: 25.3°C, Humidity: 45.2%, Location: Living Room
🚨 Broadcasted alert: Temperature too high for sensor_1

📊 Performance Results:
Messages sent: 250
Messages received: 0
Connected clients: 1
Duration: 20.00s
Throughput: 12.50 msg/sec
Active sensors: 5
Recent alerts: 3
```

## 🔍 Architecture Benefits

### ✅ **Advantages:**
- **Simple & Direct**: No broker/bridge overhead
- **Real-time**: WebSocket persistent connections
- **Scalable**: Handle multiple clients efficiently
- **Bi-directional**: Clients can send sensor data too
- **Low Latency**: Direct TCP connections

### ⚠️ **Considerations:**
- **Single Point of Failure**: Server must be highly available
- **Client State Management**: Server tracks all connections
- **Firewall/NAT**: WebSocket connections through firewalls
- **Scaling**: Limited by single server capacity

## 🌐 Client Connection

Connect to the WebSocket server:
```javascript
const ws = new WebSocket('ws://localhost:8080');

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    
    if (data.type === 'sensor_data') {
        console.log(`Sensor ${data.sensor_id}: ${data.temperature}°C`);
    } else if (data.type === 'alert') {
        console.log(`🚨 ALERT: ${data.alert_message}`);
    }
};

// Send sensor data to server
ws.send(JSON.stringify({
    type: 'sensor_data',
    sensor_id: 'sensor_external_1',
    temperature: 25.5,
    humidity: 60.0,
    location: 'External Sensor'
}));
```

## 📋 Message Format

### Sensor Data (Server → Client):
```json
{
    "type": "sensor_data",
    "sensor_id": "sensor_1",
    "temperature": 25.3,
    "humidity": 45.2,
    "location": "Living Room",
    "timestamp": 1672531200000
}
```

### Alert Data (Server → Client):
```json
{
    "type": "alert",
    "sensor_id": "sensor_1",
    "alert_message": "Temperature too high: 31.2°C (max: 27°C)",
    "temperature": 31.2,
    "humidity": 55.0,
    "location": "Living Room",
    "timestamp": 1672531200000
}
```

## 🛠️ Files

- `simple_ws_server.cpp` - WebSocket server implementation (330+ lines)
- `Makefile.simple` - Build configuration
- `README_FIXED.md` - This documentation

## 🔄 Integration with Other Approaches

This WebSocket-only approach is one of **four communication methods** being compared:

1. **MQTT-only** - Direct MQTT client ✅
2. **WebSocket-only** - Direct WebSocket server ✅ (This approach)
3. **C++ Bridge** - MQTT ↔ WebSocket bridge ✅
4. **JS Bridge** - JavaScript MQTT ↔ WebSocket bridge ✅

All approaches use the same `ThermalIsolationTracker` core for fair performance comparison. 