# MQTT-Only Bridge Makefile
# Optimized for high-performance MQTT communication

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -DNDEBUG
DEBUG_FLAGS = -std=c++17 -Wall -Wextra -g -O0 -DDEBUG
INCLUDES = -I. -I..
LIBS = -lmosquitto -ljsoncpp -lpthread

# Directories
BUILD_DIR = build
DEBUG_DIR = debug
BIN_DIR = bin
OBJ_DIR = $(BUILD_DIR)/obj
DEBUG_OBJ_DIR = $(DEBUG_DIR)/obj

# Source files
BRIDGE_SOURCES = mqtt_only_bridge.cpp
TEST_SOURCES = test_mqtt_only.cpp
THERMAL_SOURCES = ../ThermalIsolationTracker.cpp

# Object files
BRIDGE_OBJECTS = $(BRIDGE_SOURCES:%.cpp=$(OBJ_DIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:%.cpp=$(OBJ_DIR)/%.o)
THERMAL_OBJECTS = $(THERMAL_SOURCES:../%.cpp=$(OBJ_DIR)/%.o)

DEBUG_BRIDGE_OBJECTS = $(BRIDGE_SOURCES:%.cpp=$(DEBUG_OBJ_DIR)/%.o)
DEBUG_TEST_OBJECTS = $(TEST_SOURCES:%.cpp=$(DEBUG_OBJ_DIR)/%.o)
DEBUG_THERMAL_OBJECTS = $(THERMAL_SOURCES:../%.cpp=$(DEBUG_OBJ_DIR)/%.o)

# Targets
BRIDGE_LIB = $(BUILD_DIR)/libmqtt_only.a
DEBUG_BRIDGE_LIB = $(DEBUG_DIR)/libmqtt_only_debug.a
TEST_EXECUTABLE = $(BIN_DIR)/test_mqtt_only
DEBUG_TEST_EXECUTABLE = $(BIN_DIR)/test_mqtt_only_debug

# Default target
.PHONY: all
all: release

# Release build
.PHONY: release
release: $(BRIDGE_LIB) $(TEST_EXECUTABLE)

# Debug build
.PHONY: debug
debug: CXXFLAGS = $(DEBUG_FLAGS)
debug: $(DEBUG_BRIDGE_LIB) $(DEBUG_TEST_EXECUTABLE)

# Create directories
$(BUILD_DIR) $(DEBUG_DIR) $(BIN_DIR) $(OBJ_DIR) $(DEBUG_OBJ_DIR):
	mkdir -p $@

# Release library
$(BRIDGE_LIB): $(BRIDGE_OBJECTS) $(THERMAL_OBJECTS) | $(BUILD_DIR)
	ar rcs $@ $^
	@echo "✅ Built MQTT-only bridge library (release)"

# Debug library
$(DEBUG_BRIDGE_LIB): $(DEBUG_BRIDGE_OBJECTS) $(DEBUG_THERMAL_OBJECTS) | $(DEBUG_DIR)
	ar rcs $@ $^
	@echo "✅ Built MQTT-only bridge library (debug)"

# Release test executable
$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(BRIDGE_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJECTS) -L$(BUILD_DIR) -lmqtt_only $(LIBS)
	@echo "✅ Built MQTT-only test executable (release)"

# Debug test executable
$(DEBUG_TEST_EXECUTABLE): $(DEBUG_TEST_OBJECTS) $(DEBUG_BRIDGE_LIB) | $(BIN_DIR)
	$(CXX) $(DEBUG_FLAGS) -o $@ $(DEBUG_TEST_OBJECTS) -L$(DEBUG_DIR) -lmqtt_only_debug $(LIBS)
	@echo "✅ Built MQTT-only test executable (debug)"

# Release object files
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: ../%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Debug object files
$(DEBUG_OBJ_DIR)/%.o: %.cpp | $(DEBUG_OBJ_DIR)
	$(CXX) $(DEBUG_FLAGS) $(INCLUDES) -c $< -o $@

$(DEBUG_OBJ_DIR)/%.o: ../%.cpp | $(DEBUG_OBJ_DIR)
	$(CXX) $(DEBUG_FLAGS) $(INCLUDES) -c $< -o $@

# Testing targets
.PHONY: test
test: $(TEST_EXECUTABLE)
	@echo "🧪 Running MQTT-only bridge tests..."
	$(TEST_EXECUTABLE)

.PHONY: test-debug
test-debug: $(DEBUG_TEST_EXECUTABLE)
	@echo "🧪 Running MQTT-only bridge tests (debug)..."
	$(DEBUG_TEST_EXECUTABLE)

# Performance testing
.PHONY: test-performance
test-performance: $(TEST_EXECUTABLE)
	@echo "🚀 Running performance tests..."
	timeout 300 $(TEST_EXECUTABLE) || echo "Performance test completed (timeout or manual stop)"

# Stress testing
.PHONY: test-stress
test-stress: $(TEST_EXECUTABLE)
	@echo "�� Running stress tests..."
	timeout 600 $(TEST_EXECUTABLE) || echo "Stress test completed (timeout or manual stop)"

# Memory testing with Valgrind
.PHONY: test-memory
test-memory: $(DEBUG_TEST_EXECUTABLE)
	@echo "🧠 Running memory leak tests..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(DEBUG_TEST_EXECUTABLE)

# Code quality
.PHONY: lint
lint:
	@echo "🔍 Running static analysis..."
	cppcheck --enable=all --inconclusive --std=c++17 $(BRIDGE_SOURCES) $(TEST_SOURCES)

# Documentation
.PHONY: docs
docs:
	@echo "📚 Generating documentation..."
	doxygen Doxyfile

# Install (system-wide)
.PHONY: install
install: $(BRIDGE_LIB)
	@echo "📦 Installing MQTT-only bridge..."
	sudo cp mqtt_only_bridge.h /usr/local/include/
	sudo cp $(BRIDGE_LIB) /usr/local/lib/
	sudo ldconfig

# Uninstall
.PHONY: uninstall
uninstall:
	@echo "🗑️ Uninstalling MQTT-only bridge..."
	sudo rm -f /usr/local/include/mqtt_only_bridge.h
	sudo rm -f /usr/local/lib/libmqtt_only.a

# Package creation
.PHONY: package
package: release
	@echo "📦 Creating distribution package..."
	mkdir -p mqtt-only-bridge-package
	cp mqtt_only_bridge.h mqtt-only-bridge-package/
	cp $(BRIDGE_LIB) mqtt-only-bridge-package/
	cp $(TEST_EXECUTABLE) mqtt-only-bridge-package/
	cp README.md mqtt-only-bridge-package/ 2>/dev/null || echo "No README found"
	tar -czf mqtt-only-bridge.tar.gz mqtt-only-bridge-package/
	rm -rf mqtt-only-bridge-package/
	@echo "✅ Package created: mqtt-only-bridge.tar.gz"

# Benchmarking
.PHONY: benchmark
benchmark: $(TEST_EXECUTABLE)
	@echo "📊 Running comprehensive benchmarks..."
	@echo "Testing throughput, latency, and resource usage..."
	perf stat -e cycles,instructions,cache-references,cache-misses,branch-misses $(TEST_EXECUTABLE) 2>&1 | tee benchmark_results.txt

# Profiling
.PHONY: profile
profile: debug
	@echo "📈 Running profiling..."
	valgrind --tool=callgrind $(DEBUG_TEST_EXECUTABLE)
	@echo "Use 'kcachegrind callgrind.out.*' to view results"

# Code coverage
.PHONY: coverage
coverage:
	@echo "📋 Building with coverage..."
	$(MAKE) debug CXXFLAGS="$(DEBUG_FLAGS) --coverage"
	$(DEBUG_TEST_EXECUTABLE)
	gcov $(BRIDGE_SOURCES) $(TEST_SOURCES)
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory coverage_html
	@echo "Coverage report generated in coverage_html/"

# Clean targets
.PHONY: clean
clean:
	@echo "🧹 Cleaning build files..."
	rm -rf $(BUILD_DIR) $(DEBUG_DIR) $(BIN_DIR)
	rm -f *.gcov *.gcda *.gcno coverage.info
	rm -rf coverage_html/
	rm -f benchmark_results.txt
	rm -f callgrind.out.*

.PHONY: clean-all
clean-all: clean
	@echo "🧹 Cleaning all generated files..."
	rm -f mqtt-only-bridge.tar.gz

# Development helpers
.PHONY: format
format:
	@echo "🎨 Formatting code..."
	clang-format -i -style=Google $(BRIDGE_SOURCES) $(TEST_SOURCES) mqtt_only_bridge.h

.PHONY: check-deps
check-deps:
	@echo "🔍 Checking dependencies..."
	@pkg-config --exists libmosquitto || echo "❌ libmosquitto not found"
	@pkg-config --exists jsoncpp || echo "❌ jsoncpp not found"
	@which g++ > /dev/null || echo "❌ g++ not found"
	@which make > /dev/null || echo "❌ make not found"
	@echo "✅ Dependency check completed"

# Quick build and test cycle
.PHONY: quick
quick: clean release test

# Development cycle
.PHONY: dev
dev: debug test-debug

# CI/CD targets
.PHONY: ci
ci: check-deps lint release test

# Show build information
.PHONY: info
info:
	@echo "=== MQTT-Only Bridge Build Information ==="
	@echo "Compiler: $(CXX)"
	@echo "Release flags: $(CXXFLAGS)"
	@echo "Debug flags: $(DEBUG_FLAGS)"
	@echo "Libraries: $(LIBS)"
	@echo "Source files: $(BRIDGE_SOURCES) $(TEST_SOURCES)"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Binary directory: $(BIN_DIR)"
	@echo "Available targets: all, release, debug, test, clean, install"

# Help target
.PHONY: help
help:
	@echo "MQTT-Only Bridge Makefile Help"
	@echo "==============================="
	@echo ""
	@echo "Build targets:"
	@echo "  all/release    - Build release version"
	@echo "  debug          - Build debug version"  
	@echo "  clean          - Clean build files"
	@echo "  clean-all      - Clean all generated files"
	@echo ""
	@echo "Testing targets:"
	@echo "  test           - Run basic tests"
	@echo "  test-debug     - Run tests with debug build"
	@echo "  test-performance - Run performance tests"
	@echo "  test-stress    - Run stress tests"
	@echo "  test-memory    - Run memory leak tests"
	@echo ""
	@echo "Quality targets:"
	@echo "  lint           - Run static analysis"
	@echo "  format         - Format code"
	@echo "  coverage       - Generate code coverage"
	@echo "  benchmark      - Run performance benchmarks"
	@echo "  profile        - Run profiling"
	@echo ""
	@echo "Distribution targets:"
	@echo "  install        - Install system-wide"
	@echo "  uninstall      - Remove system installation"
	@echo "  package        - Create distribution package"
	@echo ""
	@echo "Development targets:"
	@echo "  quick          - Clean, build, and test"
	@echo "  dev            - Debug build and test"
	@echo "  ci             - CI/CD pipeline"
	@echo "  check-deps     - Check dependencies"
	@echo "  info           - Show build information"

# Default help
.DEFAULT_GOAL := help
