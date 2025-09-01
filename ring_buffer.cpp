#include "ring_buffer.h"
#include <iostream>

MmapRingBuffer::MmapRingBuffer(const char* name, uint32_t element_count, uint32_t element_size) 
    : name_(name) {
    // Calculate total size needed
    total_size_ = sizeof(RingBufferHeader) + element_count * element_size;
    
    // Create shared memory
    fd_ = shm_open(name_, O_CREAT | O_RDWR, 0666);
    if (fd_ == -1) {
        throw std::runtime_error("shm_open failed");
    }
    
    // Set shared memory size
    if (ftruncate(fd_, total_size_) == -1) {
        close(fd_);
        throw std::runtime_error("ftruncate failed");
    }
    
    // Map memory
    buffer_ = mmap(NULL, total_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (buffer_ == MAP_FAILED) {
        close(fd_);
        throw std::runtime_error("mmap failed");
    }
    
    // Initialize header
    header_ = static_cast<RingBufferHeader*>(buffer_);
    if (header_->size == 0) { // Initialize on first creation
        header_->head.store(0, std::memory_order_relaxed);
        header_->tail.store(0, std::memory_order_relaxed);
        header_->size = element_count;
        header_->element_size = element_size;
    }
}

MmapRingBuffer::~MmapRingBuffer() {
    if (buffer_ && buffer_ != MAP_FAILED) {
        munmap(buffer_, total_size_);
    }
    if (fd_ != -1) {
        close(fd_);
    }
}

bool MmapRingBuffer::push(const void* data) {
    uint32_t current_head = header_->head.load(std::memory_order_relaxed);
    uint32_t current_tail = header_->tail.load(std::memory_order_acquire);
    
    uint32_t next_head = (current_head + 1) % header_->size;
    
    if (next_head == current_tail) { // Buffer is full
        return false;
    }
    
    // Calculate write position
    char* element_ptr = static_cast<char*>(buffer_) + sizeof(RingBufferHeader) 
                     + current_head * header_->element_size;
    
    // Write data
    memcpy(element_ptr, data, header_->element_size);
    
    // Update head pointer with release memory ordering
    header_->head.store(next_head, std::memory_order_release);
    
    return true;
}

bool MmapRingBuffer::pop(void* data) {
    uint32_t current_tail = header_->tail.load(std::memory_order_relaxed);
    uint32_t current_head = header_->head.load(std::memory_order_acquire);
    
    if (current_tail == current_head) { // Buffer is empty
        return false;
    }
    
    // Calculate read position
    const char* element_ptr = static_cast<char*>(buffer_) + sizeof(RingBufferHeader) 
                          + current_tail * header_->element_size;
    
    // Read data
    memcpy(data, element_ptr, header_->element_size);
    
    // Update tail pointer with release memory ordering
    uint32_t next_tail = (current_tail + 1) % header_->size;
    header_->tail.store(next_tail, std::memory_order_release);
    
    return true;
}

bool MmapRingBuffer::empty() const {
    return header_->head.load(std::memory_order_acquire) == 
           header_->tail.load(std::memory_order_acquire);
}

bool MmapRingBuffer::full() const {
    uint32_t next_head = (header_->head.load(std::memory_order_relaxed) + 1) % header_->size;
    return next_head == header_->tail.load(std::memory_order_acquire);
}