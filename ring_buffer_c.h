#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// Opaque pointer to ring buffer instance
typedef void* ring_buffer_handle_t;

// C interface functions for Python integration
ring_buffer_handle_t create_ring_buffer(const char* name, uint32_t element_count, uint32_t element_size);
bool push_to_buffer(ring_buffer_handle_t handle, const void* data);
bool pop_from_buffer(ring_buffer_handle_t handle, void* data);
bool is_buffer_empty(ring_buffer_handle_t handle);
bool is_buffer_full(ring_buffer_handle_t handle);
void destroy_ring_buffer(ring_buffer_handle_t handle);

#ifdef __cplusplus
}
#endif