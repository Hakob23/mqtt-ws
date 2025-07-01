#!/usr/bin/env node

const mqtt = require('mqtt');

console.log('🚀 JavaScript Bridge Performance Test');
console.log('====================================');

let messages_sent = 0;
let messages_received = 0;
let start_time = Date.now();

// Start MQTT client
const client = mqtt.connect('mqtt://localhost:1883');

client.on('connect', () => {
    console.log('✅ Connected to MQTT broker');
    
    // Subscribe to test topic
    client.subscribe('sensors/+/data', (err) => {
        if (!err) {
            console.log('✅ Subscribed to sensor data');
            
            // Send test messages
            const sensor_ids = ['sensor_001', 'sensor_002', 'sensor_003', 'sensor_004', 'sensor_005'];
            
            const sendMessages = () => {
                sensor_ids.forEach((sensor_id, index) => {
                    const message = {
                        sensor_id: sensor_id,
                        temperature: 20 + Math.random() * 10,
                        humidity: 40 + Math.random() * 20,
                        location: 'test_room_' + index,
                        timestamp: Date.now()
                    };
                    
                    client.publish('sensors/' + sensor_id + '/data', JSON.stringify(message));
                    messages_sent++;
                });
            };
            
            // Send messages every 100ms
            const interval = setInterval(sendMessages, 100);
            
            setTimeout(() => {
                clearInterval(interval);
                const duration = (Date.now() - start_time) / 1000;
                console.log('📊 Performance Results:');
                console.log('Messages sent:', messages_sent);
                console.log('Messages received:', messages_received);
                console.log('Duration:', duration.toFixed(2) + 's');
                console.log('Throughput:', (messages_sent / duration).toFixed(2) + ' msg/sec');
                console.log('Success rate:', ((messages_received / messages_sent) * 100).toFixed(1) + '%');
                client.end();
                process.exit(0);
            }, 20000);
        }
    });
});

client.on('message', (topic, message) => {
    messages_received++;
    if (messages_received <= 5) {
        console.log('📩 Received:', topic, ':', message.toString().substring(0, 60) + '...');
    }
});

client.on('error', (error) => {
    console.error('❌ MQTT Error:', error);
}); 