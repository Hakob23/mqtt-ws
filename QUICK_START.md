# ⚡ Quick Start Guide

Get the IoT thermal monitoring system running in under 10 minutes!

## 🚀 One-Command Setup

```bash
# Clone the repository
git clone https://github.com/Hakob23/thermal-monitoring.git
cd thermal-monitoring

# Run automated setup (Linux/WSL only)
./scripts/setup.sh
```

## 🔧 Manual Setup (if automated script fails)

### 1. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev libwebsockets-dev libmosquitto-dev python3 python3-pip nodejs npm
```

**WSL2 (Windows):**
```bash
# Same as Ubuntu above
```

### 2. Build the Project
```bash
make -j$(nproc)
```

### 3. Install Node.js Dependencies
```bash
cd communication-backends/js-bridge
npm install
cd ../..
```

## 🎯 Quick Test

### Test 1: WebSocket-Only (Fastest)

**Terminal 1:**
```bash
cd communication-backends/websocket-only
./simple_ws_server
```

**Terminal 2:**
```bash
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 5 --duration 60
```

**Expected Output:**
- WebSocket server starts on port 8080
- 5 simulated sensors send temperature data
- Real-time monitoring in terminal

### Test 2: MQTT-Only (Most Reliable)

**Terminal 1:**
```bash
mosquitto -p 1883
```

**Terminal 2:**
```bash
cd communication-backends/mqtt-only
./simple_mqtt_client
```

**Terminal 3:**
```bash
cd hardware-emulation/stm32-sensors
./test_stm32_simulators --sensors 5 --duration 60
```

## 📊 Performance Comparison

Run all four architectures:

```bash
cd performance-testing/benchmarks

# Test all architectures
./bin/integration_tests --suite All --verbose --output results.json

# View results
cat results.json
```

## 🌐 Web Interface

Start the web monitoring dashboard:

```bash
cd web-interface
python3 -m http.server 8080
```

Open browser: `http://localhost:8080/test_client.html`

## 🔍 Troubleshooting

### Common Issues:

1. **"Permission denied"** → Run `chmod +x bin/*`
2. **"Port already in use"** → Kill process: `sudo kill -9 $(lsof -t -i:8080)`
3. **"Library not found"** → Install dependencies: `sudo apt install libwebsockets-dev`

### Get Help:
- Check `SETUP_GUIDE.md` for detailed instructions
- Review logs in `logs/` directory
- Open GitHub issue with error details

## 📈 What You'll See

### Performance Results (Typical):
- **WebSocket-Only**: 28ms latency, 2,200 msg/s throughput
- **MQTT-Only**: 45ms latency, 1,850 msg/s throughput  
- **C++ Bridge**: 35ms latency, 1,650 msg/s throughput
- **JS Bridge**: 52ms latency, 1,200 msg/s throughput

### Memory Usage (All <10MB):
- **MQTT-Only**: 7.8 MB
- **WebSocket-Only**: 9.2 MB
- **C++ Bridge**: 9.8 MB
- **JS Bridge**: 10 MB

## 🎯 Next Steps

1. **Explore the code**: Check individual component README files
2. **Modify configurations**: Edit JSON config files in each component
3. **Add your own sensors**: Extend the STM32 simulator
4. **Customize alerts**: Modify thermal monitoring thresholds
5. **Scale up**: Test with 100+ sensors

---

**Ready to dive deeper?** See `SETUP_GUIDE.md` for comprehensive documentation! 🚀 