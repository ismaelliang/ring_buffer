cmake_minimum_required(VERSION 3.10)
project(LockFreeRingBuffer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug build flags - for development and debugging
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wextra -Wpedantic")

# High-performance optimization flags for production scenarios
set(CMAKE_CXX_FLAGS_PROD "-O3 -march=native -mtune=native -DNDEBUG -flto -ffast-math -funroll-loops")

# Set build type to PROD by default for optimized builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE PROD)
endif()

# Enable Link Time Optimization (LTO) for better performance
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Add RT library for shared memory operations
find_library(RT_LIBRARY rt REQUIRED)

# Find pthread package
find_package(Threads REQUIRED)

# Create producer executable
add_executable(producer demo/producer.cpp)
target_include_directories(producer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/demo)
target_link_libraries(producer PRIVATE ${RT_LIBRARY} Threads::Threads)

# Create consumer executable
add_executable(consumer demo/consumer.cpp)
target_include_directories(consumer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/demo)
target_link_libraries(consumer PRIVATE ${RT_LIBRARY} Threads::Threads)

# Create check_header utility executable
add_executable(check_header demo/check_header.cpp)
target_include_directories(check_header PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/demo)
target_link_libraries(check_header PRIVATE ${RT_LIBRARY} Threads::Threads)