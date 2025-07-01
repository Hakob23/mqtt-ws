#!/bin/bash

echo "🚀 Starting MQTT-WebSocket Bridge (C++)..."

# Check if already running
if pgrep -f "mqtt_ws_bridge" > /dev/null; then
    echo "⚠️  Bridge is already running!"
    echo "Use ./stop_bridge.sh to stop it first."
    exit 1
fi

# Start the bridge
cd "$(dirname "$0")"
./bin/mqtt_ws_bridge -c ../example/config-no-ssl.json

echo "✅ Bridge started successfully!" 