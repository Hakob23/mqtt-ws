CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3
LIBS = -lmosquitto -ljsoncpp -lpthread

# Thermal monitoring source
THERMAL_SRC = ../../thermal-monitoring/ThermalIsolationTracker.cpp

# MQTT-only client
simple_mqtt_client: simple_mqtt_client.cpp $(THERMAL_SRC)
	$(CXX) $(CXXFLAGS) simple_mqtt_client.cpp $(THERMAL_SRC) -o simple_mqtt_client $(LIBS)

# Clean
clean:
	rm -f simple_mqtt_client

# Test
test: simple_mqtt_client
	./simple_mqtt_client

.PHONY: clean test 