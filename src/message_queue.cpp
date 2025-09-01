#include "message_queue.h"
#include <cstring> // For memcpy
#include <vector> // Required for std::vector

// Helper to get high-resolution timestamp (static member function implementation)
uint64_t MessageQueue::getHighResolutionTimestamp() {
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

// Constructor
MessageQueue::MessageQueue(const std::string& name, uint32_t queue_capacity, uint32_t max_payload_size)
    : max_payload_size_(max_payload_size),
      next_sequence_num_(0),
      total_message_size_(sizeof(MessageHeader) + max_payload_size),
      buffer_(name.c_str(), queue_capacity, total_message_size_) // Initialize buffer_ here
{
    if (max_payload_size_ == 0) {
        throw std::invalid_argument("max_payload_size cannot be zero.");
    }
    if (total_message_size_ > 0xFFFFFFFF) { // Check for overflow if total_message_size_ is uint32_t
        throw std::overflow_error("Total message size exceeds maximum allowed.");
    }
}

// Publish a message
bool MessageQueue::publish(MessageType type, const void* data, uint32_t data_size) {
    if (data_size > max_payload_size_) {
        throw std::invalid_argument("Payload size exceeds max_payload_size set for the queue.");
    }

    // Allocate a buffer for the full message (header + payload)
    // Note: This is a stack-allocated buffer. For very large messages, consider heap allocation or a pooled buffer.
    std::vector<char> full_message_buffer(total_message_size_);
    GenericMessage* msg = reinterpret_cast<GenericMessage*>(full_message_buffer.data());

    // Populate header
    msg->header.type = type;
    msg->header.payload_size = data_size;
    msg->header.timestamp = getHighResolutionTimestamp();
    msg->header.sequence_num = next_sequence_num_++; // Increment sequence number

    // Copy payload data
    if (data && data_size > 0) {
        std::memcpy(msg->payload, data, data_size);
    }

    // Push the full message to the underlying ring buffer
    return buffer_.push(msg);
}

// Subscribe to a message
bool MessageQueue::subscribe(void* message_buffer) {
    if (!message_buffer) {
        throw std::invalid_argument("Provided message_buffer is null.");
    }
    // Pop the full message from the underlying ring buffer
    return buffer_.pop(message_buffer);
}

// Query queue status
bool MessageQueue::empty() const {
    return buffer_.empty();
}

bool MessageQueue::full() const {
    return buffer_.full();
}

uint32_t MessageQueue::capacity() const {
    return buffer_.capacity();
}

uint32_t MessageQueue::current_size() const {
    // This needs to be calculated based on head and tail, not directly exposed by MmapRingBuffer for messages.
    // For a ring buffer, current_size = (head - tail + capacity) % capacity
    // MmapRingBuffer's current_size() method should return element count.
    // Assuming MmapRingBuffer::current_size() returns the number of elements.
    return buffer_.current_size();
}

uint32_t MessageQueue::max_payload_size() const {
    return max_payload_size_;
}
