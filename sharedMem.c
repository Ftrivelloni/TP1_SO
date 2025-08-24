#define _GNU_SOURCE // For ftruncate
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
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

void* open_shared_memory(const char* name, size_t size) {
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open open");
        exit(EXIT_FAILURE);
    }

    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap open");
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
        perror("shm_unlink");
    }
}
