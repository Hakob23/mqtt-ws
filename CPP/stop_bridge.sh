#!/bin/bash

echo "🛑 Stopping MQTT-WebSocket Bridge..."

# Find and kill the bridge process
PIDS=$(pgrep -f "mqtt_ws_bridge")

if [ -z "$PIDS" ]; then
    echo "ℹ️  No bridge process found running."
    exit 0
fi

echo "Found bridge process(es): $PIDS"

# Kill the processes
for PID in $PIDS; do
    echo "Stopping process $PID..."
    kill $PID
    sleep 1
    
    # Force kill if still running
    if kill -0 $PID 2>/dev/null; then
        echo "Force killing process $PID..."
        kill -9 $PID
    fi
done

echo "✅ Bridge stopped successfully!"

# Verify it's stopped
sleep 1
if pgrep -f "mqtt_ws_bridge" > /dev/null; then
    echo "⚠️  Some processes may still be running. Check with: ps aux | grep mqtt_ws_bridge"
else
    echo "🔍 Verified: All bridge processes stopped."
fi 