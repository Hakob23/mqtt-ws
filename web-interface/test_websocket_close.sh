#!/bin/bash

echo "Testing C++ Bridge WebSocket Connection Close..."
echo "================================================"

# Function to send WebSocket handshake and message
test_websocket() {
    echo "🔗 Connecting to C++ bridge on port 8080..."
    
    # Create WebSocket handshake request
    {
        echo -e "GET /test/topic HTTP/1.1\r"
        echo -e "Host: localhost:8080\r"
        echo -e "Upgrade: websocket\r"
        echo -e "Connection: Upgrade\r"
        echo -e "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r"
        echo -e "Sec-WebSocket-Version: 13\r"
        echo -e "\r"
        
        # Wait a moment for connection
        sleep 1
        
        # Send a test message (WebSocket frame format)
        # This is a simplified version - real WebSocket framing is more complex
        echo -e "test/topic|Hello World"
        
        # Keep connection open briefly
        sleep 2
        
        echo "🔌 Closing connection..."
        
    } | timeout 5 nc localhost 8080
    
    echo "✅ Test completed"
}

# Run the test
test_websocket

echo ""
echo "Check the C++ bridge terminal output for:"
echo "  🗑️  Removing connection for topic: test/topic"
echo "  ✅ Connection removed (Total: X)" 