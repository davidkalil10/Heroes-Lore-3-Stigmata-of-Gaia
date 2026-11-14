#pragma once

#include <stddef.h>
#include <sys/types.h>

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_SHARED 1
#define MAP_FIXED 0x10
#define MAP_NORESERVE 0
#define MAP_FAILED ((void*)-1)

#ifdef __cplusplus
extern "C" {
#endif

void* mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset);
int munmap(void* addr, size_t size);
int mprotect(void* addr, size_t size, int prot);

#ifdef __cplusplus
}
#endif

