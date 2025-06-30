# RPi4 Gateway Makefile
# Phase 2, Step 2: Raspberry Pi 4 Gateway Implementation

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
LDFLAGS = -pthread

# Directories
SRC_DIR = .
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = lib

# Include directories
INCLUDES = -I$(SRC_DIR)

# Source files
GATEWAY_SOURCES = RPi4_Gateway.cpp RPi4_DataProcessor.cpp RPi4_Components.cpp
TEST_SOURCES = test_rpi4_gateway.cpp

# Object files
GATEWAY_OBJECTS = $(GATEWAY_SOURCES:%.cpp=$(BUILD_DIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:%.cpp=$(BUILD_DIR)/%.o)

# Target executables
GATEWAY_LIB = $(LIB_DIR)/librpi4_gateway.a
TEST_EXECUTABLE = $(BIN_DIR)/test_rpi4_gateway

# Default target
.PHONY: all
all: directories $(GATEWAY_LIB) $(TEST_EXECUTABLE)

# Create directories
.PHONY: directories
directories:
	@echo "📁 Creating build directories..."
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)

# Build gateway library
$(GATEWAY_LIB): $(GATEWAY_OBJECTS)
	@echo "📦 Building RPi4 Gateway library..."
	@ar rcs $@ $^
	@echo "✅ Library built: $@"

# Build test executable
$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(GATEWAY_LIB)
	@echo "🔧 Building test executable..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Test executable built: $@"

# Compile gateway source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "🔨 Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Test targets
.PHONY: test
test: $(TEST_EXECUTABLE)
	@echo ""
	@echo "🧪 Running RPi4 Gateway Test Suite..."
	@echo "======================================="
	@$(TEST_EXECUTABLE)

.PHONY: test-demo
test-demo: $(TEST_EXECUTABLE)
	@echo ""
	@echo "🎮 Running RPi4 Gateway Interactive Demo..."
	@echo "==========================================="
	@$(TEST_EXECUTABLE) --demo

# Debug build
.PHONY: debug
debug: CXXFLAGS += -DDEBUG -g3 -O0
debug: clean all
	@echo "🐛 Debug build completed"

# Release build
.PHONY: release
release: CXXFLAGS += -DNDEBUG -O3
release: clean all
	@echo "🚀 Release build completed"

# Performance profiling build
.PHONY: profile
profile: CXXFLAGS += -pg -O2
profile: LDFLAGS += -pg
profile: clean all
	@echo "📊 Profiling build completed"

# Static analysis
.PHONY: analyze
analyze:
	@echo "🔍 Running static analysis..."
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c++17 --suppress=missingIncludeSystem $(SRC_DIR)/*.cpp $(SRC_DIR)/*.h; \
	else \
		echo "⚠️ cppcheck not found, skipping static analysis"; \
	fi

# Format code
.PHONY: format
format:
	@echo "🎨 Formatting code..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find $(SRC_DIR) -name "*.cpp" -o -name "*.h" | xargs clang-format -i; \
		echo "✅ Code formatted"; \
	else \
		echo "⚠️ clang-format not found, skipping formatting"; \
	fi

# Check build requirements
.PHONY: check-deps
check-deps:
	@echo "🔍 Checking build dependencies..."
	@echo "Compiler: $(shell $(CXX) --version | head -n1)"
	@echo "C++ Standard Library: $(shell echo '#include <iostream>' | $(CXX) -E -dM - | grep __GLIBCXX__ | head -n1)"
	@echo "System: $(shell uname -a)"
	@echo "Available CPU cores: $(shell nproc)"
	@echo "Available memory: $(shell free -h | grep '^Mem:' | awk '{print $$2}')"

# Install (for system-wide installation)
.PHONY: install
install: $(GATEWAY_LIB) $(TEST_EXECUTABLE)
	@echo "📦 Installing RPi4 Gateway..."
	@sudo mkdir -p /usr/local/lib /usr/local/include/rpi4_gateway /usr/local/bin
	@sudo cp $(GATEWAY_LIB) /usr/local/lib/
	@sudo cp $(SRC_DIR)/RPi4_Gateway.h /usr/local/include/rpi4_gateway/
	@sudo cp $(TEST_EXECUTABLE) /usr/local/bin/
	@echo "✅ Installation completed"

# Uninstall
.PHONY: uninstall
uninstall:
	@echo "🗑️ Uninstalling RPi4 Gateway..."
	@sudo rm -f /usr/local/lib/librpi4_gateway.a
	@sudo rm -rf /usr/local/include/rpi4_gateway
	@sudo rm -f /usr/local/bin/test_rpi4_gateway
	@echo "✅ Uninstallation completed"

# Package creation
.PHONY: package
package: clean all
	@echo "📦 Creating distribution package..."
	@mkdir -p dist/rpi4-gateway
	@cp -r $(SRC_DIR)/*.h $(SRC_DIR)/*.cpp Makefile README.md dist/rpi4-gateway/
	@cp -r $(LIB_DIR) $(BIN_DIR) dist/rpi4-gateway/
	@cd dist && tar -czf rpi4-gateway-$(shell date +%Y%m%d).tar.gz rpi4-gateway/
	@echo "✅ Package created: dist/rpi4-gateway-$(shell date +%Y%m%d).tar.gz"

# Documentation generation
.PHONY: docs
docs:
	@echo "📚 Generating documentation..."
	@if command -v doxygen >/dev/null 2>&1; then \
		mkdir -p docs; \
		echo "PROJECT_NAME = \"RPi4 Gateway\"" > Doxyfile.tmp; \
		echo "OUTPUT_DIRECTORY = docs" >> Doxyfile.tmp; \
		echo "INPUT = $(SRC_DIR)" >> Doxyfile.tmp; \
		echo "EXTRACT_ALL = YES" >> Doxyfile.tmp; \
		echo "GENERATE_HTML = YES" >> Doxyfile.tmp; \
		echo "GENERATE_LATEX = NO" >> Doxyfile.tmp; \
		doxygen Doxyfile.tmp; \
		rm Doxyfile.tmp; \
		echo "✅ Documentation generated in docs/"; \
	else \
		echo "⚠️ doxygen not found, skipping documentation generation"; \
	fi

# Benchmarking
.PHONY: benchmark
benchmark: $(TEST_EXECUTABLE)
	@echo "🏃 Running performance benchmarks..."
	@echo "Testing data processing throughput..."
	@time $(TEST_EXECUTABLE) --demo >/dev/null 2>&1 || true
	@echo "✅ Benchmark completed"

# Memory leak detection
.PHONY: memcheck
memcheck: $(TEST_EXECUTABLE)
	@echo "🔍 Running memory leak detection..."
	@if command -v valgrind >/dev/null 2>&1; then \
		valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes $(TEST_EXECUTABLE); \
	else \
		echo "⚠️ valgrind not found, skipping memory check"; \
	fi

# Thread safety analysis
.PHONY: thread-check
thread-check: $(TEST_EXECUTABLE)
	@echo "🧵 Running thread safety analysis..."
	@if command -v valgrind >/dev/null 2>&1; then \
		valgrind --tool=helgrind $(TEST_EXECUTABLE); \
	else \
		echo "⚠️ valgrind not found, skipping thread safety check"; \
	fi

# Code coverage
.PHONY: coverage
coverage: CXXFLAGS += --coverage
coverage: LDFLAGS += --coverage
coverage: clean all test
	@echo "📊 Generating code coverage report..."
	@if command -v gcov >/dev/null 2>&1; then \
		gcov $(GATEWAY_SOURCES) $(TEST_SOURCES); \
		if command -v lcov >/dev/null 2>&1; then \
			lcov --capture --directory . --output-file coverage.info; \
			genhtml coverage.info --output-directory coverage-html; \
			echo "✅ Coverage report generated in coverage-html/"; \
		else \
			echo "⚠️ lcov not found, basic coverage files generated"; \
		fi \
	else \
		echo "⚠️ gcov not found, skipping coverage analysis"; \
	fi

# Stress testing
.PHONY: stress-test
stress-test: $(TEST_EXECUTABLE)
	@echo "💪 Running stress tests..."
	@echo "Testing with multiple concurrent instances..."
	@for i in 1 2 3; do \
		echo "Starting stress test instance $$i..."; \
		timeout 30s $(TEST_EXECUTABLE) --demo >/dev/null 2>&1 & \
	done; \
	wait; \
	echo "✅ Stress test completed"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	@rm -f *.gcov *.gcda *.gcno coverage.info
	@rm -rf coverage-html docs dist
	@echo "✅ Clean completed"

# Full clean (including generated files)
.PHONY: distclean
distclean: clean
	@echo "🧹 Deep cleaning..."
	@rm -f core.*
	@rm -rf .deps
	@echo "✅ Deep clean completed"

# Help target
.PHONY: help
help:
	@echo "🏠 RPi4 Gateway Build System"
	@echo "============================"
	@echo ""
	@echo "Build Targets:"
	@echo "  all          - Build library and test executable (default)"
	@echo "  debug        - Build with debug symbols and no optimization"
	@echo "  release      - Build optimized release version"
	@echo "  profile      - Build with profiling support"
	@echo ""
	@echo "Test Targets:"
	@echo "  test         - Run comprehensive test suite"
	@echo "  test-demo    - Run interactive demo"
	@echo "  benchmark    - Run performance benchmarks"
	@echo "  stress-test  - Run stress testing"
	@echo ""
	@echo "Quality Targets:"
	@echo "  analyze      - Run static code analysis"
	@echo "  format       - Format code with clang-format"
	@echo "  memcheck     - Run memory leak detection"
	@echo "  thread-check - Run thread safety analysis"
	@echo "  coverage     - Generate code coverage report"
	@echo ""
	@echo "Utility Targets:"
	@echo "  check-deps   - Check build dependencies"
	@echo "  install      - Install system-wide"
	@echo "  uninstall    - Remove system installation"
	@echo "  package      - Create distribution package"
	@echo "  docs         - Generate documentation"
	@echo "  clean        - Clean build artifacts"
	@echo "  distclean    - Deep clean all generated files"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "  make                    # Build everything"
	@echo "  make test              # Run tests"
	@echo "  make test-demo         # Run interactive demo"
	@echo "  make debug test        # Debug build and test"
	@echo "  make release package   # Release build and package"

# Default make target info
.DEFAULT_GOAL := all

# Phony targets (don't correspond to files)
.PHONY: all clean distclean test test-demo debug release profile analyze format \
        check-deps install uninstall package docs benchmark memcheck thread-check \
        coverage stress-test help directories 