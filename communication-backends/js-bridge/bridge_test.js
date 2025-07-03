#!/usr/bin/env node

const Bridge = require('./lib/mqtt-ws');
const mqtt = require('mqtt');
const WebSocket = require('ws');

console.log('🚀 JavaScript Bridge Performance Test');
console.log('====================================');

let messages_sent = 0;
let messages_received = 0;
let start_time = Date.now();

// Create the bridge
const bridge = Bridge.createBridge({
    mqtt: {
        host: 'localhost',
        port: 1883
    },
    websocket: {
        port: 8081  // Use different port to avoid conflicts
    }
});

// Connect MQTT client to bridge
const mqttClient = bridge.connectMqtt();

// Create WebSocket client to connect to bridge
const wsClient = new WebSocket('ws://localhost:8081');

// Set up MQTT client
mqttClient.on('connect', () => {
    console.log('✅ MQTT client connected to bridge');
    
    // Subscribe to test topic
    mqttClient.subscribe('test/+/data', (err) => {
        if (!err) {
            console.log('✅ MQTT client subscribed to test data');
        }
    });
});

mqttClient.on('message', (topic, message) => {
    messages_received++;
    if (messages_received <= 5) {
        console.log('📩 MQTT received:', topic, ':', message.toString().substring(0, 60) + '...');
    }
});

// Set up WebSocket client
wsClient.on('open', () => {
    console.log('✅ WebSocket client connected to bridge');
    
    // Send test messages via WebSocket
    const sendMessages = () => {
        for (let i = 1; i <= 5; i++) {
            const message = {
                sensor_id: `sensor_${i.toString().padStart(3, '0')}`,
                temperature: 20 + Math.random() * 10,
                humidity: 40 + Math.random() * 20,
                location: `test_room_${i}`,
                timestamp: Date.now()
            };
            
            wsClient.send(JSON.stringify(message));
            messages_sent++;
        }
    };
    
    // Send messages every 100ms
    const interval = setInterval(sendMessages, 100);
    
    setTimeout(() => {
        clearInterval(interval);
        const duration = (Date.now() - start_time) / 1000;
        console.log('📊 Bridge Performance Results:');
        console.log('Messages sent via WebSocket:', messages_sent);
        console.log('Messages received via MQTT:', messages_received);
        console.log('Duration:', duration.toFixed(2) + 's');
        console.log('Throughput:', (messages_sent / duration).toFixed(2) + ' msg/sec');
        console.log('Success rate:', ((messages_received / messages_sent) * 100).toFixed(1) + '%');
        
        // Cleanup
        wsClient.close();
        mqttClient.end();
        bridge.close();
        process.exit(0);
    }, 20000);
});

wsClient.on('error', (error) => {
    console.error('❌ WebSocket Error:', error);
});

bridge.on('error', (error) => {
    console.error('❌ Bridge Error:', error);
});

// Handle cleanup on exit
process.on('SIGINT', () => {
    console.log('\n🛑 Shutting down...');
    wsClient.close();
    mqttClient.end();
    bridge.close();
    process.exit(0);
}); 