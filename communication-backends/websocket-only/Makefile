CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I../../thermal-monitoring -I/usr/include/jsoncpp
LIBS = -lwebsockets -ljsoncpp -lpthread

TARGET = simple_ws_server
SOURCES = simple_ws_server.cpp ../../thermal-monitoring/ThermalIsolationTracker.cpp

.PHONY: all clean install-deps test run

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -f $(TARGET)

install-deps:
	@echo "🔧 Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y libwebsockets-dev libjsoncpp-dev

test: $(TARGET)
	@echo "🧪 Running WebSocket-Only test..."
	./$(TARGET)

run: $(TARGET)
	@echo "🚀 Starting WebSocket-Only Thermal Server..."
	./$(TARGET)

help:
	@echo "Available targets:"
	@echo "  all         - Build the WebSocket server"
	@echo "  clean       - Clean build files"
	@echo "  install-deps- Install required dependencies"
	@echo "  test        - Build and run test"
	@echo "  run         - Build and run server"
	@echo "  help        - Show this help" 