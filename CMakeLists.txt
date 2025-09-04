cmake_minimum_required(VERSION 3.10)

# vcpkg integration - MUST be set before project()
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

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

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_PROD ${CMAKE_BINARY_DIR}/bin)

# Add RT library for shared memory operations
find_library(RT_LIBRARY rt REQUIRED)

# Find pthread package

find_package(Threads REQUIRED)

# Find Google Test package using CONFIG mode (vcpkg provides config files)
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    find_package(GTest REQUIRED)
endif()

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


# Enable testing
enable_testing()

# Create test executable with Google Test
add_executable(ringbuffer_tests test/test_ringbuffer.cpp)
target_include_directories(ringbuffer_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/demo)
target_link_libraries(ringbuffer_tests PRIVATE ${RT_LIBRARY} Threads::Threads GTest::gtest GTest::gtest_main)
set_target_properties(ringbuffer_tests PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/test
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/test
    RUNTIME_OUTPUT_DIRECTORY_PROD ${CMAKE_BINARY_DIR}/test)

# Create no_create test executable with Google Test
add_executable(no_create_tests test/test_no_create.cpp)
target_include_directories(no_create_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/demo)
target_link_libraries(no_create_tests PRIVATE ${RT_LIBRARY} Threads::Threads GTest::gtest GTest::gtest_main)
set_target_properties(no_create_tests PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/test
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/test
    RUNTIME_OUTPUT_DIRECTORY_PROD ${CMAKE_BINARY_DIR}/test)

# Add the tests
add_test(NAME RingBufferTests COMMAND ringbuffer_tests)
add_test(NAME NoCreateTests COMMAND no_create_tests)
