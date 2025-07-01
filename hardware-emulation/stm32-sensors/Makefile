# STM32 Sensor Node Simulator Makefile
# Phase 2 - Distributed Architecture Implementation

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread
DEBUGFLAGS := -g -O0 -DDEBUG
INCLUDES := -I.

# Source files
SIMULATOR_SOURCES := STM32_SensorNode.cpp
TEST_SOURCES := test_stm32_simulators.cpp

# Object files
SIMULATOR_OBJECTS := $(SIMULATOR_SOURCES:.cpp=.o)
TEST_OBJECTS := $(TEST_SOURCES:.cpp=.o)

# Targets
SIMULATOR_LIB := libstm32_sim.a
TEST_EXECUTABLE := test_stm32_simulators

# Default target
all: $(SIMULATOR_LIB) $(TEST_EXECUTABLE)

# Build static library
$(SIMULATOR_LIB): $(SIMULATOR_OBJECTS)
	@echo "🔗 Building STM32 simulator library..."
	ar rcs $@ $^
	@echo "✅ Library $(SIMULATOR_LIB) created successfully"

# Build test executable
$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(SIMULATOR_LIB)
	@echo "🔗 Building test executable..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< -L. -lstm32_sim
	@echo "✅ Test executable $(TEST_EXECUTABLE) created successfully"

# Compile source files
%.o: %.cpp
	@echo "🔨 Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Debug build
debug: CXXFLAGS += $(DEBUGFLAGS)
debug: clean $(SIMULATOR_LIB) $(TEST_EXECUTABLE)
	@echo "🐛 Debug build completed"

# Test targets
test: $(TEST_EXECUTABLE)
	@echo "🧪 Running STM32 simulator tests..."
	./$(TEST_EXECUTABLE)

test-quick: $(TEST_EXECUTABLE)
	@echo "⚡ Running quick STM32 simulator test..."
	timeout 30s ./$(TEST_EXECUTABLE) || true

# Utility targets
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f *.o $(SIMULATOR_LIB) $(TEST_EXECUTABLE)
	rm -f *.log *.txt
	@echo "✅ Clean completed"

install: $(SIMULATOR_LIB)
	@echo "📦 Installing STM32 simulator library..."
	mkdir -p /usr/local/include/stm32_simulation
	mkdir -p /usr/local/lib
	cp STM32_SensorNode.h /usr/local/include/stm32_simulation/
	cp $(SIMULATOR_LIB) /usr/local/lib/
	@echo "✅ Installation completed"

uninstall:
	@echo "🗑️ Uninstalling STM32 simulator library..."
	rm -rf /usr/local/include/stm32_simulation
	rm -f /usr/local/lib/$(SIMULATOR_LIB)
	@echo "✅ Uninstallation completed"

# Documentation target
docs:
	@echo "📚 Generating documentation..."
	@echo "STM32 Sensor Node Simulator" > README.txt
	@echo "============================" >> README.txt
	@echo "" >> README.txt
	@echo "This library simulates STM32 microcontrollers with environmental sensors." >> README.txt
	@echo "" >> README.txt
	@echo "Features:" >> README.txt
	@echo "- Multiple sensor types (DHT22, BME280, SHT30, DS18B20)" >> README.txt
	@echo "- Communication protocols (UART, MQTT, SPI, I2C)" >> README.txt
	@echo "- Environmental patterns (Indoor, Outdoor, Industrial, etc.)" >> README.txt
	@echo "- Fault simulation and power management" >> README.txt
	@echo "- Multi-threaded real-time operation" >> README.txt
	@echo "" >> README.txt
	@echo "Build:" >> README.txt
	@echo "  make all    - Build library and test executable" >> README.txt
	@echo "  make test   - Run comprehensive tests" >> README.txt
	@echo "  make debug  - Build with debug symbols" >> README.txt
	@echo "  make clean  - Clean build artifacts" >> README.txt
	@echo "" >> README.txt
	@echo "Usage:" >> README.txt
	@echo "  ./test_stm32_simulators  - Run interactive demo" >> README.txt
	@echo "" >> README.txt
	@echo "Generated on: $$(date)" >> README.txt
	@echo "✅ Documentation generated as README.txt"

# Analysis targets
analyze-deps:
	@echo "🔍 Analyzing dependencies..."
	@echo "External dependencies:"
	@grep -h "#include <" *.h *.cpp | sort | uniq
	@echo ""
	@echo "Internal dependencies:"
	@grep -h "#include \"" *.h *.cpp | sort | uniq

analyze-size: $(SIMULATOR_LIB) $(TEST_EXECUTABLE)
	@echo "📊 Analyzing binary sizes..."
	@echo "Library size:"
	@ls -lh $(SIMULATOR_LIB)
	@echo "Test executable size:"
	@ls -lh $(TEST_EXECUTABLE)
	@echo "Object file sizes:"
	@ls -lh *.o

# Benchmark target
benchmark: $(TEST_EXECUTABLE)
	@echo "🏃 Running performance benchmark..."
	@echo "This will run a stress test with multiple sensors..."
	timeout 60s ./$(TEST_EXECUTABLE) > benchmark_results.txt 2>&1 || true
	@echo "✅ Benchmark completed, results saved to benchmark_results.txt"

# Help target
help:
	@echo "STM32 Sensor Node Simulator Build System"
	@echo "========================================"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build library and test executable (default)"
	@echo "  debug        - Build with debug symbols"
	@echo "  test         - Run comprehensive tests"
	@echo "  test-quick   - Run quick test (30 seconds max)"
	@echo "  clean        - Clean build artifacts"
	@echo "  install      - Install library system-wide"
	@echo "  uninstall    - Remove system-wide installation"
	@echo "  docs         - Generate documentation"
	@echo "  analyze-deps - Show dependency analysis"
	@echo "  analyze-size - Show binary size analysis"
	@echo "  benchmark    - Run performance benchmark"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make clean && make debug && make test"
	@echo "  make test-quick"
	@echo "  make benchmark"
	@echo ""
	@echo "Files generated:"
	@echo "  libstm32_sim.a      - Static library"
	@echo "  test_stm32_simulators - Test executable"
	@echo "  *.log, *.txt        - Test and log files"

# Force rebuild
rebuild: clean all

# Check for required tools
check-tools:
	@echo "🔧 Checking required tools..."
	@which $(CXX) > /dev/null || (echo "❌ C++ compiler not found"; exit 1)
	@which ar > /dev/null || (echo "❌ ar (archiver) not found"; exit 1)
	@echo "✅ All required tools are available"

# Dependency tracking
STM32_SensorNode.o: STM32_SensorNode.h STM32_SensorNode.cpp
test_stm32_simulators.o: STM32_SensorNode.h test_stm32_simulators.cpp

# Phony targets
.PHONY: all debug test test-quick clean install uninstall docs analyze-deps analyze-size benchmark help rebuild check-tools

# Default goal
.DEFAULT_GOAL := all 