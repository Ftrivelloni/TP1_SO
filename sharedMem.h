#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdbool.h>
#include <semaphore.h>
#include <sys/types.h>
#include "structs.h"  


// Functions for shared memory operations
void* create_shared_memory(const char* name, size_t size);
void* open_shared_memory(const char* name, size_t size, int flags);
void close_shared_memory(void* ptr, const char* name, size_t size);

#define GAME_STATE_SHM NAME_BOARD
#define GAME_SYNC_SHM NAME_SYNC

#endif // SHARED_MEM_H