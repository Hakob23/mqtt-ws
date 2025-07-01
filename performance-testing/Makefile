CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -pthread
LIBS = -lmosquitto -ljsoncpp

# Thermal monitoring source
THERMAL_SRC = ../thermal-monitoring/ThermalIsolationTracker.cpp

# Performance test executable
PERF_TEST = mqtt_performance_test

# Default target
all: $(PERF_TEST)

# Build performance test
$(PERF_TEST): mqtt_performance_test.cpp $(THERMAL_SRC)
	$(CXX) $(CXXFLAGS) mqtt_performance_test.cpp $(THERMAL_SRC) -o $(PERF_TEST) $(LIBS)

# Run performance test with 10 sensors
test-10-sensors: $(PERF_TEST)
	@echo "🧪 Running MQTT performance test with 10 sensors..."
	./$(PERF_TEST) 10 60 100

# Run performance test with custom parameters
test-custom: $(PERF_TEST)
	@echo "🧪 Running MQTT performance test with custom parameters..."
	@echo "Usage: make test-custom SENSORS=10 DURATION=60 INTERVAL=100"
	./$(PERF_TEST) $(SENSORS) $(DURATION) $(INTERVAL)

# Run quick test (30 seconds)
test-quick: $(PERF_TEST)
	@echo "⚡ Running quick MQTT performance test..."
	./$(PERF_TEST) 10 30 100

# Run stress test (2 minutes, high frequency)
test-stress: $(PERF_TEST)
	@echo "🔥 Running stress test..."
	./$(PERF_TEST) 10 120 50

# Clean build artifacts
clean:
	rm -f $(PERF_TEST) *.o *.log mqtt_performance_results.txt

# Install dependencies
install-deps:
	@echo "📦 Installing required dependencies..."
	sudo apt update
	sudo apt install -y build-essential cmake git
	sudo apt install -y mosquitto mosquitto-clients libmosquitto-dev
	sudo apt install -y libjsoncpp-dev

# Check if MQTT broker is running
check-broker:
	@echo "🔍 Checking MQTT broker status..."
	@if pgrep mosquitto > /dev/null; then \
		echo "✅ Mosquitto broker is running"; \
	else \
		echo "❌ Mosquitto broker is not running"; \
		echo "   Start with: sudo systemctl start mosquitto"; \
		exit 1; \
	fi

# Start MQTT broker (if not running)
start-broker:
	@echo "🚀 Starting MQTT broker..."
	@if ! pgrep mosquitto > /dev/null; then \
		sudo systemctl start mosquitto; \
		echo "✅ Mosquitto broker started"; \
	else \
		echo "✅ Mosquitto broker already running"; \
	fi

# Run complete test suite
test-suite: check-broker all
	@echo "🧪 Running complete test suite..."
	@echo "1. Quick test (30s)..."
	@make test-quick
	@echo ""
	@echo "2. Standard test (60s)..."
	@make test-10-sensors
	@echo ""
	@echo "3. Stress test (120s)..."
	@make test-stress
	@echo ""
	@echo "✅ All tests completed!"

# Show help
help:
	@echo "MQTT Performance Testing Framework"
	@echo "=================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build performance test executable"
	@echo "  test-10-sensors  - Run test with 10 sensors (60s)"
	@echo "  test-quick       - Run quick test (30s)"
	@echo "  test-stress      - Run stress test (120s, high frequency)"
	@echo "  test-custom      - Run with custom parameters"
	@echo "  test-suite       - Run complete test suite"
	@echo "  check-broker     - Check if MQTT broker is running"
	@echo "  start-broker     - Start MQTT broker"
	@echo "  install-deps     - Install required dependencies"
	@echo "  clean            - Clean build artifacts"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Custom test usage:"
	@echo "  make test-custom SENSORS=10 DURATION=60 INTERVAL=100"
	@echo ""
	@echo "Examples:"
	@echo "  make test-10-sensors"
	@echo "  make test-quick"
	@echo "  make test-stress"

.PHONY: all test-10-sensors test-custom test-quick test-stress clean install-deps check-broker start-broker test-suite help 