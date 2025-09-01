cmake_minimum_required(VERSION 3.10)
project(LockFreeRingBuffer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug build flags - for development and debugging
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wextra -Wpedantic")

# High-performance optimization flags for low-latency scenarios
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -DNDEBUG -flto -ffast-math")

# Optional: Ultra-fast build type for maximum performance
set(CMAKE_CXX_FLAGS_ULTRAFAST "-O3 -march=native -mtune=native -DNDEBUG -flto -ffast-math -funroll-loops")

# Set build type to Release by default for optimized builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Enable Link Time Optimization (LTO) for better performance
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Add RT library for shared memory operations
find_library(RT_LIBRARY rt REQUIRED)

# Find pthread package
find_package(Threads REQUIRED)

# Create ring buffer library
add_library(ring_buffer
    ring_buffer.cpp
    ring_buffer.h
)
target_link_libraries(ring_buffer PRIVATE ${RT_LIBRARY} Threads::Threads)

# Create shared ring buffer library with C interface for Python
add_library(ring_buffer_shared SHARED
    ring_buffer.cpp
    ring_buffer.h
    ring_buffer_c.cpp
    ring_buffer_c.h
)
target_link_libraries(ring_buffer_shared PRIVATE ${RT_LIBRARY} Threads::Threads)
set_target_properties(ring_buffer_shared PROPERTIES OUTPUT_NAME "ring_buffer")

# Create producer executable
add_executable(producer producer.cpp)
target_link_libraries(producer PRIVATE ring_buffer ${RT_LIBRARY} Threads::Threads)

# Create consumer executable
add_executable(consumer consumer.cpp)
target_link_libraries(consumer PRIVATE ring_buffer ${RT_LIBRARY} Threads::Threads)