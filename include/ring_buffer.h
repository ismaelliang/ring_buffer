#pragma once

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <stdexcept>

// Ring buffer header information with cache line alignment to prevent false sharing
struct alignas(64) RingBufferHeader {
    std::atomic<uint32_t> head;  // Write position
    std::atomic<uint32_t> tail;  // Read position
    uint32_t size;               // Total buffer size (number of elements)
    uint32_t element_size;       // Size of each element
};

// Ring buffer class using mmap for shared memory
class MmapRingBuffer {
public:
    MmapRingBuffer(const char* name, uint32_t element_count, uint32_t element_size);
    ~MmapRingBuffer();
    
    // Disable copy and assignment
    MmapRingBuffer(const MmapRingBuffer&) = delete;
    MmapRingBuffer& operator=(const MmapRingBuffer&) = delete;
    
    bool push(const void* data);
    bool pop(void* data);
    bool empty() const;
    bool full() const;

    uint32_t capacity() const; // Returns the total number of elements the buffer can hold
    uint32_t current_size() const; // Returns the current number of elements in the buffer
    
private:
    void* buffer_;
    RingBufferHeader* header_;
    int fd_;
    const char* name_;
    size_t total_size_;
};