#include "../include/mqtt_ws_bridge.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/resource.h>
#include <fstream>

namespace mqtt_ws {

// Global bridge instance for libwebsockets callback
static MqttWebSocketBridge* g_bridge_instance = nullptr;

//=============================================================================
// MessageBuffer Implementation
//=============================================================================

MessageBuffer::MessageBuffer(size_t initial_capacity) 
    : buffer_(initial_capacity), size_(0), capacity_(initial_capacity) {
}

void MessageBuffer::resize(size_t new_size) {
    if (new_size > capacity_) {
        buffer_.resize(new_size);
        capacity_ = new_size;
    }
    size_ = new_size;
}

bool MessageBuffer::parse_websocket_message(std::string& topic, std::vector<uint8_t>& payload) {
    if (size_ < 4) return false; // Minimum for UTF-16LE topic separator
    
    // Parse UTF-16LE encoded topic followed by '|'
    size_t offset = 0;
    topic.clear();
    
    while (offset < size_ - 1) {
        uint16_t char_code = *reinterpret_cast<uint16_t*>(buffer_.data() + offset);
        
        if (char_code == '|') {
            offset += 2;
            break;
        }
        
        // Convert UTF-16LE to ASCII (simplified)
        if (char_code < 128) {
            topic += static_cast<char>(char_code);
        }
        offset += 2;
    }
    
    // Remaining bytes are the payload
    if (offset < size_) {
        payload.assign(buffer_.begin() + offset, buffer_.begin() + size_);
    } else {
        payload.clear();
    }
    
    return !topic.empty();
}

void MessageBuffer::format_mqtt_message(const std::string& topic, const std::vector<uint8_t>& payload) {
    // Format: "topic|" in UTF-16LE + payload
    size_t topic_utf16_size = (topic.length() + 1) * 2; // +1 for '|'
    size_t total_size = topic_utf16_size + payload.size();
    
    resize(total_size);
    
    // Convert topic to UTF-16LE
    uint16_t* utf16_ptr = reinterpret_cast<uint16_t*>(buffer_.data());
    for (size_t i = 0; i < topic.length(); ++i) {
        utf16_ptr[i] = static_cast<uint16_t>(topic[i]);
    }
    utf16_ptr[topic.length()] = '|';
    
    // Copy payload
    if (!payload.empty()) {
        std::memcpy(buffer_.data() + topic_utf16_size, payload.data(), payload.size());
    }
}

// ... (rest of implementation will be added in separate files)

} // namespace mqtt_ws
