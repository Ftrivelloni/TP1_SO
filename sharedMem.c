#define _GNU_SOURCE // For ftruncate
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "sharedMem.h"

void* create_shared_memory(const char* name, size_t size) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open create");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap create");
        exit(EXIT_FAILURE);
    }

    close(fd);
    return ptr;
}

void* open_shared_memory(const char* name, size_t size, int flags) {
    int fd;
    int retries = 0;
    const int max_retries = 5;
    
    while (retries < max_retries) {
        fd = shm_open(name, flags, 0666);
        if (fd != -1) {
            break;  // Success
        }
        
        if (errno != EACCES && errno != EPERM) {
            // If not a permission error, fail immediately
            perror("shm_open open");
            exit(EXIT_FAILURE);
        }
        
        fprintf(stderr, "Permission denied on shm_open. Retry %d/%d\n", 
                retries + 1, max_retries);
        sleep(1);  // Wait before retrying
        retries++;
    }
    
    if (fd == -1) {
        perror("shm_open open (all retries failed)");
        exit(EXIT_FAILURE);
    }

    // Determine protection flags based on access mode
    int prot = PROT_READ;
    if ((flags & O_RDWR) || (flags & O_WRONLY)) {
        prot |= PROT_WRITE;
    }

    void* ptr = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap open");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
    return ptr;
}

void close_shared_memory(void* ptr, const char* name, size_t size) {
    if (munmap(ptr, size) == -1) {
        perror("munmap");
    }

    if (shm_unlink(name) == -1) {
        // Ignore errors if we don't have permission to unlink
        if (errno != EACCES && errno != EPERM) {
            perror("shm_unlink");
        }
    }
}