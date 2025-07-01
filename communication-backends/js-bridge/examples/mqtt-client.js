#!/usr/bin/env node

const mqtt = require('mqtt');
const readline = require('readline');

// MQTT client configuration
const MQTT_BROKER_URL = 'mqtt://localhost:1883';
const DEFAULT_TOPIC = 'test/topic';

class MQTTClient {
    constructor() {
        this.client = null;
        this.subscribedTopics = new Set();
        this.rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout
        });
    }

    connect() {
        console.log(`Connecting to MQTT broker at ${MQTT_BROKER_URL}...`);
        
        this.client = mqtt.connect(MQTT_BROKER_URL);

        this.client.on('connect', () => {
            console.log('✅ Connected to MQTT broker');
            this.showHelp();
            this.startInteractiveMode();
        });

        this.client.on('message', (topic, message) => {
            const timestamp = new Date().toLocaleTimeString();
            console.log(`\n📥 [${timestamp}] Received on topic "${topic}": ${message.toString()}`);
            this.showPrompt();
        });

        this.client.on('error', (error) => {
            console.error('❌ MQTT Error:', error);
        });

        this.client.on('close', () => {
            console.log('❌ MQTT connection closed');
        });
    }

    subscribe(topic) {
        if (this.subscribedTopics.has(topic)) {
            console.log(`Already subscribed to topic: ${topic}`);
            return;
        }

        this.client.subscribe(topic, (err) => {
            if (err) {
                console.error(`❌ Failed to subscribe to ${topic}:`, err);
            } else {
                this.subscribedTopics.add(topic);
                console.log(`✅ Subscribed to topic: ${topic}`);
            }
        });
    }

    unsubscribe(topic) {
        if (!this.subscribedTopics.has(topic)) {
            console.log(`Not subscribed to topic: ${topic}`);
            return;
        }

        this.client.unsubscribe(topic, (err) => {
            if (err) {
                console.error(`❌ Failed to unsubscribe from ${topic}:`, err);
            } else {
                this.subscribedTopics.delete(topic);
                console.log(`✅ Unsubscribed from topic: ${topic}`);
            }
        });
    }

    publish(topic, message) {
        this.client.publish(topic, message, (err) => {
            if (err) {
                console.error(`❌ Failed to publish to ${topic}:`, err);
            } else {
                const timestamp = new Date().toLocaleTimeString();
                console.log(`📤 [${timestamp}] Published to topic "${topic}": ${message}`);
            }
        });
    }

    showHelp() {
        console.log('\n=== MQTT Client Commands ===');
        console.log('sub <topic>     - Subscribe to a topic');
        console.log('unsub <topic>   - Unsubscribe from a topic');
        console.log('pub <topic> <message> - Publish a message to a topic');
        console.log('list            - List subscribed topics');
        console.log('help            - Show this help');
        console.log('exit            - Exit the client');
        console.log('\nExamples:');
        console.log('  sub test/topic');
        console.log('  pub test/topic Hello World');
        console.log('  pub test/topic "Hello from MQTT!"');
        console.log('');
    }

    showPrompt() {
        this.rl.question('mqtt> ', (input) => {
            this.handleCommand(input.trim());
        });
    }

    handleCommand(input) {
        if (!input) {
            this.showPrompt();
            return;
        }

        const parts = input.split(' ');
        const command = parts[0].toLowerCase();

        switch (command) {
            case 'sub':
                if (parts.length < 2) {
                    console.log('Usage: sub <topic>');
                } else {
                    this.subscribe(parts[1]);
                }
                break;

            case 'unsub':
                if (parts.length < 2) {
                    console.log('Usage: unsub <topic>');
                } else {
                    this.unsubscribe(parts[1]);
                }
                break;

            case 'pub':
                if (parts.length < 3) {
                    console.log('Usage: pub <topic> <message>');
                } else {
                    const topic = parts[1];
                    const message = parts.slice(2).join(' ');
                    this.publish(topic, message);
                }
                break;

            case 'list':
                if (this.subscribedTopics.size === 0) {
                    console.log('No subscribed topics');
                } else {
                    console.log('Subscribed topics:');
                    this.subscribedTopics.forEach(topic => {
                        console.log(`  - ${topic}`);
                    });
                }
                break;

            case 'help':
                this.showHelp();
                break;

            case 'exit':
            case 'quit':
                this.disconnect();
                return;

            default:
                console.log(`Unknown command: ${command}. Type 'help' for available commands.`);
                break;
        }

        this.showPrompt();
    }

    startInteractiveMode() {
        console.log(`\nStarting interactive mode. Type 'help' for commands.`);
        
        // Auto-subscribe to default topic for demo
        this.subscribe(DEFAULT_TOPIC);
        
        this.showPrompt();
    }

    disconnect() {
        console.log('Disconnecting...');
        if (this.client) {
            this.client.end();
        }
        this.rl.close();
        process.exit(0);
    }
}

// Handle Ctrl+C gracefully
process.on('SIGINT', () => {
    console.log('\nReceived SIGINT. Disconnecting...');
    process.exit(0);
});

// Start the MQTT client
const client = new MQTTClient();
client.connect(); 