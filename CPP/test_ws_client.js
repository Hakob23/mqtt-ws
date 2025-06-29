const WebSocket = require('ws');

console.log('🧪 Testing C++ MQTT-WebSocket Bridge...');

// Connect to the C++ bridge
const ws = new WebSocket('ws://localhost:8080/test/topic');

ws.on('open', function open() {
    console.log('✅ Connected to C++ bridge!');
    
    // Send a test message
    const testMessage = 'test/topic|Hello from Node.js client!';
    console.log('📤 Sending:', testMessage);
    ws.send(testMessage);
    
    // Keep connection alive for a moment
    setTimeout(() => {
        console.log('🔌 Closing connection...');
        ws.close();
    }, 2000);
});

ws.on('message', function message(data) {
    console.log('📨 Received:', data.toString());
});

ws.on('close', function close(code, reason) {
    console.log('🔌 Connection closed:', code, reason.toString());
    process.exit(0);
});

ws.on('error', function error(err) {
    console.error('❌ WebSocket error:', err.message);
    process.exit(1);
});

// Timeout after 10 seconds
setTimeout(() => {
    console.log('⏰ Test timeout');
    process.exit(1);
}, 10000); 