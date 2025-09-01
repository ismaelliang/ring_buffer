#pragma once

#include <cstdint>
#include <chrono> // For high-resolution timestamps
#include <string>

// Enum for different message types (can be extended by users)
enum class MessageType : uint32_t {
    UNKNOWN = 0,
    MARKET_DATA,
    ORDER_UPDATE,
    HEARTBEAT,
    // Add more message types here as your application grows
};

// Generic Message Header
// This header will precede every message in the buffer
struct MessageHeader {
    MessageType type;        // Type of the message
    uint32_t payload_size;   // Actual size of the data following this header
    uint64_t timestamp;      // High-resolution timestamp when the message was created (nanoseconds)
    uint64_t sequence_num;   // Unique sequence number for ordering and tracking
};

// Generic Message structure for fixed-size messages
// The total size of a message will be sizeof(MessageHeader) + max_payload_size
// Users will typically cast the 'payload' buffer to their specific message struct
struct alignas(64) GenericMessage { // Align to cache line if beneficial for performance
    MessageHeader header;
    // Placeholder for actual message data. The size of this array will be determined
    // by the 'max_payload_size' argument during MessageQueue creation.
    // This is a flexible array member concept, but for fixed-size, it's just a buffer.
    char payload[1]; // Actual size determined at runtime by 'max_payload_size' of queue
};
