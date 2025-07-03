#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <mosquitto.h>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <jsoncpp/json/json.h>

using namespace std;

// WebSocket client type
typedef websocketpp::client<websocketpp::config::asio_tls_client> WebSocketClient;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

class BridgeTest {
private:
    struct mosquitto* mqtt_client_;
    WebSocketClient ws_client_;
    websocketpp::connection_hdl ws_connection_;
    
    int messages_sent_ = 0;
    int messages_received_ = 0;
    chrono::steady_clock::time_point start_time_;
    bool ws_connected_ = false;
    bool mqtt_connected_ = false;
    
public:
    BridgeTest() : mqtt_client_(nullptr) {
        start_time_ = chrono::steady_clock::now();
        mosquitto_lib_init();
        
        // Initialize WebSocket client
        ws_client_.init_asio();
        ws_client_.set_tls_init_handler([](websocketpp::connection_hdl) {
            return websocketpp::lib::make_shared<boost::asio::ssl::context>(
                boost::asio::ssl::context::tlsv12);
        });
        
        ws_client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            ws_connection_ = hdl;
            ws_connected_ = true;
            cout << "✅ WebSocket client connected to bridge" << endl;
        });
        
        ws_client_.set_message_handler([this](websocketpp::connection_hdl hdl, message_ptr msg) {
            messages_received_++;
            if (messages_received_ <= 5) {
                cout << "📩 WebSocket received: " << msg->get_payload().substr(0, 60) << "..." << endl;
            }
        });
        
        ws_client_.set_fail_handler([](websocketpp::connection_hdl hdl) {
            cout << "❌ WebSocket connection failed" << endl;
        });
    }
    
    ~BridgeTest() {
        if (mqtt_client_) {
            mosquitto_destroy(mqtt_client_);
        }
        mosquitto_lib_cleanup();
    }
    
    bool Start() {
        // Connect to MQTT broker
        mqtt_client_ = mosquitto_new("bridge_test_client", true, this);
        if (!mqtt_client_) {
            cerr << "❌ Failed to create MQTT client" << endl;
            return false;
        }
        
        mosquitto_connect_callback_set(mqtt_client_, OnMQTTConnect);
        mosquitto_message_callback_set(mqtt_client_, OnMQTTMessage);
        
        if (mosquitto_connect(mqtt_client_, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS) {
            cerr << "❌ Failed to connect to MQTT broker" << endl;
            return false;
        }
        
        // Connect to WebSocket server
        websocketpp::lib::error_code ec;
        WebSocketClient::connection_ptr con = ws_client_.get_connection("ws://localhost:8080", ec);
        if (ec) {
            cerr << "❌ Failed to create WebSocket connection: " << ec.message() << endl;
            return false;
        }
        
        ws_client_.connect(con);
        
        // Start WebSocket client
        thread ws_thread([this]() {
            ws_client_.run();
        });
        ws_thread.detach();
        
        cout << "🚀 Bridge Test Started" << endl;
        return true;
    }
    
    void RunTest(int duration_seconds = 20) {
        cout << "🔄 Running bridge test for " << duration_seconds << " seconds..." << endl;
        
        auto end_time = chrono::steady_clock::now() + chrono::seconds(duration_seconds);
        
        while (chrono::steady_clock::now() < end_time) {
            // Send messages via WebSocket every 100ms
            if (ws_connected_) {
                SendTestMessage();
            }
            
            // Process MQTT events
            if (mqtt_client_) {
                mosquitto_loop(mqtt_client_, 10, 1);
            }
            
            this_thread::sleep_for(chrono::milliseconds(100));
        }
        
        PrintResults();
    }
    
private:
    void SendTestMessage() {
        static random_device rd;
        static mt19937 gen(rd());
        static uniform_real_distribution<> temp_dist(20.0, 30.0);
        static uniform_real_distribution<> humidity_dist(40.0, 60.0);
        
        Json::Value message;
        message["sensor_id"] = "sensor_" + to_string((messages_sent_ % 5) + 1);
        message["temperature"] = temp_dist(gen);
        message["humidity"] = humidity_dist(gen);
        message["location"] = "test_room_" + to_string((messages_sent_ % 5) + 1);
        message["timestamp"] = chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count();
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        string payload = Json::writeString(builder, message);
        
        try {
            ws_client_.send(ws_connection_, payload, websocketpp::frame::opcode::text);
            messages_sent_++;
        } catch (const exception& e) {
            cerr << "❌ Failed to send WebSocket message: " << e.what() << endl;
        }
    }
    
    void PrintResults() {
        auto now = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(now - start_time_);
        double duration_sec = duration.count() / 1000.0;
        
        cout << "\n📊 Bridge Test Results:" << endl;
        cout << "Messages sent via WebSocket: " << messages_sent_ << endl;
        cout << "Messages received via MQTT: " << messages_received_ << endl;
        cout << "Duration: " << fixed << setprecision(2) << duration_sec << "s" << endl;
        cout << "Throughput: " << fixed << setprecision(2) << (messages_sent_ / duration_sec) << " msg/sec" << endl;
        cout << "Success rate: " << fixed << setprecision(1) << ((messages_received_ / (double)messages_sent_) * 100) << "%" << endl;
    }
    
    static void OnMQTTConnect(struct mosquitto* mosq, void* userdata, int result) {
        BridgeTest* test = static_cast<BridgeTest*>(userdata);
        if (result == 0) {
            cout << "✅ MQTT client connected to bridge" << endl;
            test->mqtt_connected_ = true;
            
            // Subscribe to test topic
            mosquitto_subscribe(mosq, nullptr, "test/+/data", 1);
        } else {
            cerr << "❌ MQTT connection failed: " << mosquitto_connack_string(result) << endl;
        }
    }
    
    static void OnMQTTMessage(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* msg) {
        BridgeTest* test = static_cast<BridgeTest*>(userdata);
        test->messages_received_++;
        
        string topic = msg->topic;
        string payload(static_cast<char*>(msg->payload), msg->payloadlen);
        
        if (test->messages_received_ <= 5) {
            cout << "📩 MQTT received: " << topic << " : " << payload.substr(0, 60) << "..." << endl;
        }
    }
};

int main() {
    cout << "🚀 C++ Bridge Performance Test" << endl;
    cout << "==============================" << endl;
    
    BridgeTest test;
    
    if (!test.Start()) {
        return 1;
    }
    
    // Wait a moment for connections to establish
    this_thread::sleep_for(chrono::seconds(2));
    
    // Run the test
    test.RunTest(20);
    
    cout << "✅ C++ Bridge test completed!" << endl;
    return 0;
} 