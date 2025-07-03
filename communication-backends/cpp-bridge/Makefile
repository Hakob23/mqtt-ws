# MQTT-WebSocket Bridge Makefile
# Optimized for Linux

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -pthread
INCLUDES = -Iinclude
LIBS = -lwebsockets -lmosquitto -lssl -lcrypto -ljsoncpp

# Directories
SRC_DIR = src
INC_DIR = include
BIN_DIR = bin
OBJ_DIR = build

# Source files (exclude demo.cpp from main build)
SOURCES = $(filter-out $(SRC_DIR)/demo.cpp, $(wildcard $(SRC_DIR)/*.cpp))
THERMAL_SOURCES = ../../thermal-monitoring/ThermalIsolationTracker.cpp
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o) $(OBJ_DIR)/ThermalIsolationTracker.o

# Target binary
TARGET = $(BIN_DIR)/mqtt_ws_bridge

# Default target
all: $(TARGET)

# Create directories
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Build target
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LIBS)
	@echo "✅ Build complete: $@"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile ThermalIsolationTracker
$(OBJ_DIR)/ThermalIsolationTracker.o: $(THERMAL_SOURCES) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Install dependencies (Ubuntu/Debian)
deps:
	sudo apt update
	sudo apt install -y libwebsockets-dev libmosquitto-dev libssl-dev libjsoncpp-dev

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "🧹 Cleaned build files"

# Run the bridge
run: $(TARGET)
	./$(TARGET) -c ../example/config.json

# Build and run simple demo
demo: $(OBJ_DIR)/demo.o | $(BIN_DIR)
	$(CXX) $(OBJ_DIR)/demo.o -o $(BIN_DIR)/demo
	./$(BIN_DIR)/demo

# Build thermal system test
test-thermal: $(OBJ_DIR)/test_thermal_system.o $(OBJ_DIR)/ThermalIsolationTracker.o | $(BIN_DIR)
	$(CXX) $(OBJ_DIR)/test_thermal_system.o $(OBJ_DIR)/ThermalIsolationTracker.o -o $(BIN_DIR)/test_thermal_system $(LIBS)
	./$(BIN_DIR)/test_thermal_system

$(OBJ_DIR)/test_thermal_system.o: test_thermal_system.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Debug build
debug: CXXFLAGS += -g -DDEBUG -O0
debug: $(TARGET)

# Memory check with valgrind
memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET) -c ../example/config.json

# Check library dependencies
check-deps:
	@echo "📋 Checking dependencies..."
	@pkg-config --exists libwebsockets && echo "✅ libwebsockets found" || echo "❌ libwebsockets missing"
	@pkg-config --exists libmosquitto && echo "✅ libmosquitto found" || echo "❌ libmosquitto missing"
	@pkg-config --exists openssl && echo "✅ OpenSSL found" || echo "❌ OpenSSL missing"
	@which g++ > /dev/null && echo "✅ g++ found" || echo "❌ g++ missing"

# Show compilation info
info:
	@echo "🔧 Build Configuration:"
	@echo "  Compiler: $(CXX)"
	@echo "  Flags: $(CXXFLAGS)"
	@echo "  Libraries: $(LIBS)"
	@echo "  Sources: $(SOURCES)"

.PHONY: all clean run debug memcheck check-deps deps info
