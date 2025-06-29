# MQTT-WebSocket Bridge Examples

This directory contains example clients to test your MQTT-WebSocket bridge.

## Prerequisites

1. **MQTT Broker**: Mosquitto is running on `localhost:1883`
2. **Bridge**: The MQTT-WebSocket bridge is running on `localhost:8080` (HTTPS)

## Example Clients

### 1. WebSocket Client (HTML)

**File**: `websocket-client.html`

A web-based client that connects to the bridge via WebSocket.

**Usage**:

1. Open `websocket-client.html` in your web browser
2. **Important**: Since we're using self-signed certificates, you need to accept the certificate first:
   - Go to `https://localhost:8080` in your browser
   - Accept the security warning/add exception
3. Enter a topic (default: `test/topic`)
4. Click "Connect"
5. Send messages using the input field

### 2. MQTT Client (Node.js)

**File**: `mqtt-client.js`

A command-line MQTT client for publishing and subscribing to topics.

**Usage**:

```bash
# Run the MQTT client
node examples/mqtt-client.js

# Available commands:
mqtt> sub test/topic          # Subscribe to a topic
mqtt> pub test/topic Hello!   # Publish a message
mqtt> list                    # List subscribed topics
mqtt> help                    # Show help
mqtt> exit                    # Exit
```

## Testing the Bridge

To test that the bridge is working correctly:

### Option 1: MQTT → WebSocket

1. Open the **WebSocket client** in your browser
2. Connect to topic `test/topic`
3. Run the **MQTT client** in terminal:
   ```bash
   node examples/mqtt-client.js
   ```
4. In the MQTT client, publish a message:
   ```
   mqtt> pub test/topic Hello from MQTT!
   ```
5. You should see the message appear in the WebSocket client

### Option 2: WebSocket → MQTT

1. Run the **MQTT client** and subscribe to a topic:
   ```bash
   node examples/mqtt-client.js
   # In the client:
   mqtt> sub test/topic
   ```
2. Open the **WebSocket client** in your browser
3. Connect to the same topic (`test/topic`)
4. Send a message from the WebSocket client
5. You should see the message appear in the MQTT client terminal

### Option 3: Using Mosquitto Command Line Tools

You can also use the built-in mosquitto tools:

```bash
# Subscribe to messages (in one terminal)
mosquitto_sub -h localhost -t "test/topic"

# Publish messages (in another terminal)
mosquitto_pub -h localhost -t "test/topic" -m "Hello from mosquitto!"
```

## How It Works

```
MQTT Client ←→ MQTT Broker ←→ Bridge ←→ WebSocket Client
     ↑              ↑           ↑            ↑
  Port 1883    Port 1883   Port 8080   Browser/JS
```

1. **MQTT clients** connect directly to the MQTT broker (port 1883)
2. **WebSocket clients** connect to the bridge (port 8080)
3. The **bridge** subscribes to MQTT topics based on the WebSocket URL path
4. Messages flow bidirectionally through the bridge

## WebSocket URL Format

The WebSocket client connects using this URL format:
```
wss://localhost:8080/<topic_name>
```

Examples:
- `wss://localhost:8080/test/topic`
- `wss://localhost:8080/sensors/temperature`
- `wss://localhost:8080/chat/room1`

## Troubleshooting

### SSL Certificate Issues

If you get SSL certificate errors in the browser:

1. Navigate to `https://localhost:8080` directly
2. Accept the security warning/add exception
3. Then try connecting with the WebSocket client

### Connection Refused

If you get "connection refused" errors:

1. Check that Mosquitto is running:
   ```bash
   sudo systemctl status mosquitto
   ```

2. Check that the bridge is running:
   ```bash
   ps aux | grep mqttwsBridge
   ```

3. Check the bridge logs for any errors

### Port Already in Use

If port 8080 is already in use, you can change it in `example/config.json`:

```json
{
    "websocket": {
        "port": 8081,
        ...
    }
}
``` 