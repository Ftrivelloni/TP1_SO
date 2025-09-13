#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdbool.h>
#include <semaphore.h>
#include <sys/types.h>
#include "structs.h"  


// Functions for shared memory operations

/**
 * @brief Create a new shared memory segment and map it to process memory.
 * @param name The name of the shared memory segment.
 * @param size The size of the shared memory segment in bytes.
 * @return Pointer to the mapped shared memory region.
 */
void* create_shared_memory(const char* name, size_t size);

/**
 * @brief Open an existing shared memory segment and map it to process memory.
 * @param name The name of the shared memory segment.
 * @param size The size of the shared memory segment in bytes.
 * @param flags File access flags (O_RDONLY, O_RDWR, etc.).
 * @return Pointer to the mapped shared memory region.
 */
void* open_shared_memory(const char* name, size_t size, int flags);

/**
 * @brief Unmap and unlink a shared memory segment.
 * @param ptr Pointer to the mapped shared memory region.
 * @param name The name of the shared memory segment.
 * @param size The size of the shared memory segment in bytes.
 */
void close_shared_memory(void* ptr, const char* name, size_t size);

/* Convenient aliases for the shared memory segment names */
#define GAME_STATE_SHM NAME_BOARD
#define GAME_SYNC_SHM NAME_SYNC

#endif // SHARED_MEM_H