# Integration Testing Makefile for MQTT-WebSocket IoT System
# Phase 2, Step 3: Comprehensive Integration Testing

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
DEBUG_FLAGS = -g -DDEBUG -O0
RELEASE_FLAGS = -O3 -DNDEBUG
PROFILE_FLAGS = -pg -O2

# Directories
SRC_DIR = .
STM32_DIR = ../STM32-Simulators
RPI4_DIR = ../RPi4-Gateway
BRIDGE_DIR = ../../Bridges/CPP-bridge
THERMAL_DIR = ../..
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = lib

# Include paths
INCLUDES = -I$(SRC_DIR) -I$(STM32_DIR) -I$(RPI4_DIR) -I$(BRIDGE_DIR)/include -I$(THERMAL_DIR)

# Library paths and libraries
LIBPATHS = -L$(STM32_DIR) -L$(RPI4_DIR)/lib -L$(BRIDGE_DIR)/lib
LIBS = -lpthread -lmosquitto -ljsoncpp
# LIBS = -lstm32_sim -lrpi4_gateway -lmqtt_ws_bridge -lpthread -lmosquitto -ljsoncpp

# Source files
INTEGRATION_SOURCES = IntegrationTestController.cpp \
                     IntegrationTestImplementations.cpp \
                     ComponentManagers.cpp \
                     main_integration_tests.cpp

# Object files
INTEGRATION_OBJECTS = $(INTEGRATION_SOURCES:%.cpp=$(BUILD_DIR)/%.o)

# Test executables
INTEGRATION_TEST_EXEC = $(BIN_DIR)/integration_tests
QUICK_TEST_EXEC = $(BIN_DIR)/quick_integration_test
STRESS_TEST_EXEC = $(BIN_DIR)/stress_test
PERFORMANCE_TEST_EXEC = $(BIN_DIR)/performance_test

# Default target
.PHONY: all
all: $(INTEGRATION_TEST_EXEC)

# Create directories
$(BUILD_DIR) $(BIN_DIR) $(LIB_DIR):
	mkdir -p $@

# Debug build
.PHONY: debug
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(INTEGRATION_TEST_EXEC)

# Release build
.PHONY: release
release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(INTEGRATION_TEST_EXEC)

# Profile build
.PHONY: profile
profile: CXXFLAGS += $(PROFILE_FLAGS)
profile: $(INTEGRATION_TEST_EXEC)

# Build dependencies first
.PHONY: deps
deps:
	@echo "Skipping dependencies for integration test build..."
	# $(MAKE) -C $(STM32_DIR) all
	# $(MAKE) -C $(RPI4_DIR) all
	# $(MAKE) -C $(BRIDGE_DIR) all

# Main integration test executable
$(INTEGRATION_TEST_EXEC): $(INTEGRATION_OBJECTS) | $(BIN_DIR) deps
	@echo "Linking integration test executable..."
	$(CXX) $(CXXFLAGS) $(INTEGRATION_OBJECTS) $(LIBPATHS) $(LIBS) -o $@
	@echo "Integration test executable built successfully: $@"

# Component managers implementation
$(BUILD_DIR)/ComponentManagers.o: ComponentManagers.cpp ComponentManagers.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Integration test controller
$(BUILD_DIR)/IntegrationTestController.o: IntegrationTestController.cpp IntegrationTestController.h ComponentManagers.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Test implementations
$(BUILD_DIR)/IntegrationTestImplementations.o: IntegrationTestImplementations.cpp IntegrationTestController.h ComponentManagers.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Main test program
$(BUILD_DIR)/main_integration_tests.o: main_integration_tests.cpp IntegrationTestController.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Test targets
.PHONY: test-all
test-all: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Complete Integration Test Suite ==="
	./$(INTEGRATION_TEST_EXEC) --suite Complete --verbose

.PHONY: test-basic
test-basic: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Basic Integration Tests ==="
	./$(INTEGRATION_TEST_EXEC) --suite Basic --verbose

.PHONY: test-performance
test-performance: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Performance Tests ==="
	./$(INTEGRATION_TEST_EXEC) --suite Performance --verbose

.PHONY: test-reliability
test-reliability: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Reliability Tests ==="
	./$(INTEGRATION_TEST_EXEC) --suite Reliability --verbose

.PHONY: test-e2e
test-e2e: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running End-to-End Test ==="
	./$(INTEGRATION_TEST_EXEC) --test EndToEndDataFlow --verbose

.PHONY: test-stress
test-stress: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Stress Test ==="
	./$(INTEGRATION_TEST_EXEC) --test StressLoad --verbose --config stress_config.json

.PHONY: test-thermal
test-thermal: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Thermal Integration Test ==="
	./$(INTEGRATION_TEST_EXEC) --test ThermalIntegration --verbose

.PHONY: test-fault-tolerance
test-fault-tolerance: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Fault Tolerance Test ==="
	./$(INTEGRATION_TEST_EXEC) --test FaultTolerance --verbose

# Quick tests for development
.PHONY: quick-test
quick-test: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Quick Integration Test ==="
	./$(INTEGRATION_TEST_EXEC) --test EndToEndDataFlow --duration 10s

# Continuous integration test
.PHONY: ci-test
ci-test: clean deps test-basic
	@echo "=== Continuous Integration Tests Completed ==="

# Performance benchmarking
.PHONY: benchmark
benchmark: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Performance Benchmark ==="
	./$(INTEGRATION_TEST_EXEC) --test PerformanceBenchmark --verbose --output benchmark_results.json

# Memory leak testing
.PHONY: test-memory
test-memory: debug
	@echo "=== Running Memory Leak Tests ==="
	valgrind --leak-check=full --show-leak-kinds=all ./$(INTEGRATION_TEST_EXEC) --test EndToEndDataFlow --duration 30s

# Static analysis
.PHONY: analyze
analyze:
	@echo "=== Running Static Analysis ==="
	cppcheck --enable=all --std=c++17 $(INTEGRATION_SOURCES) $(INCLUDES)

# Coverage analysis
.PHONY: coverage
coverage: CXXFLAGS += --coverage
coverage: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Coverage Analysis ==="
	./$(INTEGRATION_TEST_EXEC) --suite Basic
	gcov $(INTEGRATION_SOURCES)
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory coverage_html

# Stress testing with different configurations
.PHONY: stress-test-light
stress-test-light: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Light Stress Test ==="
	./$(INTEGRATION_TEST_EXEC) --test StressLoad --sensors 25 --duration 60s --rate 2x

.PHONY: stress-test-heavy
stress-test-heavy: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Heavy Stress Test ==="  
	./$(INTEGRATION_TEST_EXEC) --test StressLoad --sensors 100 --duration 300s --rate 10x

# Development and debugging targets
.PHONY: debug-run
debug-run: debug
	@echo "=== Running Debug Session ==="
	gdb --args ./$(INTEGRATION_TEST_EXEC) --test EndToEndDataFlow --verbose

.PHONY: profile-run
profile-run: profile
	@echo "=== Running Performance Profiling ==="
	./$(INTEGRATION_TEST_EXEC) --test PerformanceBenchmark
	gprof ./$(INTEGRATION_TEST_EXEC) gmon.out > profile_analysis.txt

# System requirements check
.PHONY: check-requirements
check-requirements:
	@echo "=== Checking System Requirements ==="
	@which mosquitto || echo "WARNING: mosquitto broker not found"
	@which gcc || echo "ERROR: gcc compiler not found"
	@which g++ || echo "ERROR: g++ compiler not found"
	@echo "Checking for required libraries..."
	@pkg-config --exists libmosquitto && echo "✓ libmosquitto found" || echo "✗ libmosquitto not found"
	@pkg-config --exists jsoncpp && echo "✓ jsoncpp found" || echo "✗ jsoncpp not found"

# Environment setup
.PHONY: setup-env
setup-env:
	@echo "=== Setting Up Test Environment ==="
	mkdir -p logs test_data results
	chmod +x scripts/*.sh 2>/dev/null || true

# Component health check
.PHONY: health-check
health-check: $(INTEGRATION_TEST_EXEC)
	@echo "=== Component Health Check ==="
	./$(INTEGRATION_TEST_EXEC) --health-check

# Test data generation
.PHONY: generate-test-data
generate-test-data:
	@echo "=== Generating Test Data ==="
	mkdir -p test_data
	./scripts/generate_test_scenarios.sh test_data/

# System monitoring during tests
.PHONY: monitor-test
monitor-test: $(INTEGRATION_TEST_EXEC)
	@echo "=== Running Tests with System Monitoring ==="
	./scripts/monitor_system.sh &
	./$(INTEGRATION_TEST_EXEC) --suite Performance --verbose
	pkill -f monitor_system.sh

# Log analysis
.PHONY: analyze-logs
analyze-logs:
	@echo "=== Analyzing Test Logs ==="
	find logs/ -name "*.log" -exec echo "=== {} ===" \; -exec tail -20 {} \;

# Clean targets
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	rm -f *.gcov *.gcda *.gcno coverage.info gmon.out profile_analysis.txt
	rm -rf coverage_html

.PHONY: clean-logs
clean-logs:
	@echo "Cleaning log files..."
	rm -rf logs/*.log test_data/*.json results/*.json

.PHONY: clean-all
clean-all: clean clean-logs
	@echo "Full cleanup completed"

# Documentation
.PHONY: docs
docs:
	@echo "=== Generating Documentation ==="
	doxygen Doxyfile 2>/dev/null || echo "Doxygen not available"

# Installation targets
.PHONY: install
install: $(INTEGRATION_TEST_EXEC)
	@echo "=== Installing Integration Tests ==="
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(INTEGRATION_TEST_EXEC) $(DESTDIR)/usr/local/bin/

.PHONY: uninstall
uninstall:
	@echo "=== Uninstalling Integration Tests ==="
	rm -f $(DESTDIR)/usr/local/bin/integration_tests

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all              - Build integration test executable"
	@echo "  debug            - Build with debug flags"
	@echo "  release          - Build with optimization"
	@echo "  profile          - Build with profiling support"
	@echo ""
	@echo "Test targets:"
	@echo "  test-all         - Run complete test suite"
	@echo "  test-basic       - Run basic integration tests"
	@echo "  test-performance - Run performance tests"
	@echo "  test-reliability - Run reliability tests"
	@echo "  test-e2e         - Run end-to-end test"
	@echo "  test-stress      - Run stress test"
	@echo "  test-thermal     - Run thermal integration test"
	@echo "  quick-test       - Run quick development test"
	@echo ""
	@echo "Analysis targets:"
	@echo "  benchmark        - Run performance benchmark"
	@echo "  test-memory      - Run memory leak test"
	@echo "  analyze          - Run static analysis"
	@echo "  coverage         - Generate code coverage report"
	@echo ""
	@echo "Utility targets:"
	@echo "  clean            - Clean build artifacts"
	@echo "  check-requirements - Check system requirements"
	@echo "  setup-env        - Setup test environment"
	@echo "  health-check     - Check component health"
	@echo "  help             - Show this help message"

# Make sure intermediate files are not deleted
.PRECIOUS: $(BUILD_DIR)/%.o

# Pattern rule for object files
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ 