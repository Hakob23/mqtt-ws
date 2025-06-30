/*!
 * mqtt-ws: a node.js MQTT/WebSocket bridge
 * Copyright(c) 2013 M2M Connections, Inc <info@m2mconnectionsinc.com>
 * MIT Licensed
 */

var mqtt = require('mqtt'),
    WebSocketServer = require('ws').Server,
    events = require('events'),
    util = require('util'),
    https = require('https'),
    http = require('http'),
    fs = require('fs'),
    underscore = require('underscore');

var defaultOptions = {
    mqtt: {
        host: "localhost",
        port: 1883,
    },
    websocket: {
        port: 8080,
        ssl: false
    }
};

var Bridge = module.exports = function Bridge(options) {
    options = options || {mqtt: {}, websocket: {}};
    this.options = {};
    this.options.mqtt = underscore.extend(defaultOptions.mqtt, options.mqtt);
    this.options.websocket = underscore.extend(defaultOptions.websocket, options.websocket);
    this.port = this.options.websocket.port;

    var self = this;

    // Check if SSL is configured and certificates exist
    var useSSL = this.options.websocket.ssl && 
                 this.options.websocket.ssl_key && 
                 this.options.websocket.ssl_cert;
    
    if (useSSL) {
        try {
            // Verify SSL files exist
            if (!fs.existsSync(this.options.websocket.ssl_key)) {
                console.warn('SSL key file not found:', this.options.websocket.ssl_key);
                useSSL = false;
            }
            if (!fs.existsSync(this.options.websocket.ssl_cert)) {
                console.warn('SSL cert file not found:', this.options.websocket.ssl_cert);
                useSSL = false;
            }
        } catch (err) {
            console.warn('SSL configuration error:', err.message);
            useSSL = false;
        }
    }

    // Create appropriate server (HTTP or HTTPS)
    var app;
    if (useSSL) {
        console.log('Starting HTTPS WebSocket server on port', this.port);
        app = https.createServer({
            key: fs.readFileSync(this.options.websocket.ssl_key),
            cert: fs.readFileSync(this.options.websocket.ssl_cert)
        }, function(req, res){
            res.writeHead(418);
            res.end("<h1>418 - I'm a teapot</h1>");
        }).listen(this.port);
    } else {
        console.log('Starting HTTP WebSocket server on port', this.port);
        app = http.createServer(function(req, res){
            res.writeHead(418);
            res.end("<h1>418 - I'm a teapot</h1>");
        }).listen(this.port);
    }

    // Create WebSocket Server
    this.wss = new WebSocketServer({server: app});
    this.wss.on('error', function(err) {
        console.error('WebSocket Server Error:', err);
        self.emit('error', err);
    });

    // Incoming WS connection
    this.wss.on('connection', function(ws, req) {
        // Set connection string we can use as client identifier
        // Handle both old and new ws API versions
        var remoteAddress, remotePort;
        
        if (req) {
            // Newer ws versions pass req as second parameter
            remoteAddress = req.connection ? req.connection.remoteAddress : req.socket.remoteAddress;
            remotePort = req.connection ? req.connection.remotePort : req.socket.remotePort;
        } else if (ws.upgradeReq) {
            // Older ws versions
            remoteAddress = ws.upgradeReq.connection.remoteAddress;
            remotePort = ws.upgradeReq.connection.remotePort;
        } else {
            // Fallback
            remoteAddress = 'unknown';
            remotePort = 0;
        }

        ws.connectString = util.format("%s:%d", remoteAddress, remotePort);
        console.log('WebSocket connection from:', ws.connectString);

        // Signal we've got a connection
        self.emit('connection', ws, req);
    });
    
    events.EventEmitter.call(this);
};
util.inherits(Bridge, events.EventEmitter);

Bridge.prototype.connectMqtt = function(options) {
    // Create our client using the modern API
    options = options || {};
    
    // Build connection URL
    var mqttUrl = util.format('mqtt://%s:%d', this.options.mqtt.host, this.options.mqtt.port);
    
    // Set encoding option
    options.encoding = options.encoding || "binary";
    
    console.log('Connecting to MQTT broker at:', mqttUrl);
    var mqttClient = mqtt.connect(mqttUrl, options);
    
    mqttClient.options = options;
    mqttClient.host = this.options.mqtt.host;
    mqttClient.port = this.options.mqtt.port;

    // Add connection event handlers
    mqttClient.on('connect', function() {
        console.log('Connected to MQTT broker');
    });

    mqttClient.on('error', function(err) {
        console.error('MQTT connection error:', err);
    });

    mqttClient.on('close', function() {
        console.log('MQTT connection closed');
    });

    return mqttClient;
}

Bridge.prototype.close = function() {
    console.log('Closing WebSocket server');
    this.wss.close();
    this.emit('close');
}

module.exports.createBridge = function(options) {
    return new Bridge(options);
};

