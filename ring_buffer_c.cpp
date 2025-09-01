#include "ring_buffer_c.h"
#include "ring_buffer.h"
#include <new>
#include <iostream>

extern "C" {

ring_buffer_handle_t create_ring_buffer(const char* name, uint32_t element_count, uint32_t element_size) {
    try {
        MmapRingBuffer* buffer = new MmapRingBuffer(name, element_count, element_size);
        return static_cast<ring_buffer_handle_t>(buffer);
    } catch (const std::exception& e) {
        std::cerr << "Error creating ring buffer: " << e.what() << std::endl;
        return nullptr;
    }
}

bool push_to_buffer(ring_buffer_handle_t handle, const void* data) {
    if (!handle || !data) {
        return false;
    }
    
    try {
        MmapRingBuffer* buffer = static_cast<MmapRingBuffer*>(handle);
        return buffer->push(data);
    } catch (const std::exception& e) {
        std::cerr << "Error pushing to buffer: " << e.what() << std::endl;
        return false;
    }
}

bool pop_from_buffer(ring_buffer_handle_t handle, void* data) {
    if (!handle || !data) {
        return false;
    }
    
    try {
        MmapRingBuffer* buffer = static_cast<MmapRingBuffer*>(handle);
        return buffer->pop(data);
    } catch (const std::exception& e) {
        std::cerr << "Error popping from buffer: " << e.what() << std::endl;
        return false;
    }
}

bool is_buffer_empty(ring_buffer_handle_t handle) {
    if (!handle) {
        return true;
    }
    
    try {
        MmapRingBuffer* buffer = static_cast<MmapRingBuffer*>(handle);
        return buffer->empty();
    } catch (const std::exception& e) {
        std::cerr << "Error checking if buffer is empty: " << e.what() << std::endl;
        return true;
    }
}

bool is_buffer_full(ring_buffer_handle_t handle) {
    if (!handle) {
        return false;
    }
    
    try {
        MmapRingBuffer* buffer = static_cast<MmapRingBuffer*>(handle);
        return buffer->full();
    } catch (const std::exception& e) {
        std::cerr << "Error checking if buffer is full: " << e.what() << std::endl;
        return false;
    }
}

void destroy_ring_buffer(ring_buffer_handle_t handle) {
    if (!handle) {
        return;
    }
    
    try {
        MmapRingBuffer* buffer = static_cast<MmapRingBuffer*>(handle);
        delete buffer;
    } catch (const std::exception& e) {
        std::cerr << "Error destroying ring buffer: " << e.what() << std::endl;
    }
}

}