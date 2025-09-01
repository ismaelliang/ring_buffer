#include "ring_buffer.h"
#include <iostream>

MmapRingBuffer::MmapRingBuffer(const char* name, uint32_t element_count, uint32_t element_size) 
    : name_(name) {
    std::cout << "MmapRingBuffer: Creating/attaching to buffer: " << name << std::endl;
    std::cout << "  element_count: " << element_count << ", element_size: " << element_size << std::endl;

    // Calculate total size needed
    total_size_ = sizeof(RingBufferHeader) + element_count * element_size;
    std::cout << "  Calculated total_size_: " << total_size_ << std::endl;
    
    // Create shared memory
    fd_ = shm_open(name_, O_CREAT | O_RDWR, 0666);
    if (fd_ == -1) {
        throw std::runtime_error("shm_open failed");
    }
    std::cout << "  shm_open successful, fd_: " << fd_ << std::endl;
    
    // Set shared memory size
    if (ftruncate(fd_, total_size_) == -1) {
        close(fd_);
        throw std::runtime_error("ftruncate failed");
    }
    std::cout << "  ftruncate successful, size: " << total_size_ << std::endl;
    
    // Map memory
    buffer_ = mmap(NULL, total_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (buffer_ == MAP_FAILED) {
        close(fd_);
        throw std::runtime_error("mmap failed");
    }
    std::cout << "  mmap successful, buffer_ address: " << buffer_ << std::endl;
    
    // Initialize header
    header_ = static_cast<RingBufferHeader*>(buffer_);
    if (header_->size == 0) { // Initialize on first creation
        std::cout << "  Initializing new buffer header." << std::endl;
        header_->head.store(0, std::memory_order_relaxed);
        header_->tail.store(0, std::memory_order_relaxed);
        header_->size = element_count;
        header_->element_size = element_size;
    }
    std::cout << "  Header initialized. header_->size: " << header_->size 
              << ", header_->element_size: " << header_->element_size << std::endl;
}

MmapRingBuffer::~MmapRingBuffer() {
    if (buffer_ && buffer_ != MAP_FAILED) {
        munmap(buffer_, total_size_);
    }
    if (fd_ != -1) {
        close(fd_);
        shm_unlink(name_); // Unlink the shared memory object
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

uint32_t MmapRingBuffer::capacity() const {
    return header_->size;
}

uint32_t MmapRingBuffer::current_size() const {
    uint32_t current_head = header_->head.load(std::memory_order_acquire);
    uint32_t current_tail = header_->tail.load(std::memory_order_acquire);
    if (current_head >= current_tail) {
        return current_head - current_tail;
    } else {
        return header_->size - current_tail + current_head;
    }
}