#pragma once

#include "ring_buffer.h" // Includes MmapRingBuffer
#include "message_types.h" // Includes MessageHeader and MessageType
#include <string>
#include <stdexcept>

class MessageQueue {
public:
    // Constructor to create or attach to a named message queue.
    // - name: A unique identifier for the shared memory queue.
    // - queue_capacity: The maximum number of *messages* (not bytes) the queue can hold.
    // - max_payload_size: The maximum size in bytes of the user's data (payload) per message.
    //                     The actual element size in the underlying ring buffer will be
    //                     sizeof(MessageHeader) + max_payload_size.
    MessageQueue(const std::string& name, uint32_t queue_capacity, uint32_t max_payload_size);

    // Destructor (handles shared memory cleanup)
    ~MessageQueue() = default;

    // Publish a message to the queue.
    // - type: The MessageType of the message.
    // - data: Pointer to the user's raw message payload.
    // - data_size: The actual size of the user's payload. Must be <= max_payload_size.
    // Returns true if the message was successfully published, false if the queue is full.
    bool publish(MessageType type, const void* data, uint32_t data_size);

    // Subscribe to a message from the queue.
    // - message_buffer: A buffer provided by the caller to receive the full GenericMessage
    //                   (including header and payload). The buffer must be large enough
    //                   to hold sizeof(MessageHeader) + max_payload_size.
    // Returns true if a message was successfully retrieved, false if the queue is empty.
    bool subscribe(void* message_buffer); // Use void* for generic buffer

    // Query queue status
    bool empty() const;
    bool full() const;
    uint32_t capacity() const;        // Returns queue_capacity (number of messages)
    uint32_t current_size() const;    // Returns current number of messages in the queue
    uint32_t max_payload_size() const; // Returns the max_payload_size set at creation

private:
    // Helper to get high-resolution timestamp
    static uint64_t getHighResolutionTimestamp();

    uint32_t max_payload_size_;       // Max payload size for this queue instance
    uint32_t total_message_size_;     // sizeof(MessageHeader) + max_payload_size
    MmapRingBuffer buffer_;           // The underlying ring buffer
    uint64_t next_sequence_num_;      // For assigning unique sequence numbers to published messages (per instance)
};
