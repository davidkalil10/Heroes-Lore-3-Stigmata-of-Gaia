// Heap-backed non-executable mmap adapter for Unicorn TCI on Nintendo Switch.
#include <sys/mman.h>
#include <errno.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Hl3Mapping {
    void* address;
    size_t size;
    struct Hl3Mapping* next;
} Hl3Mapping;

static Hl3Mapping* g_mappings = NULL;

void* mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
    if (addr || fd != -1 || offset || prot != (PROT_READ | PROT_WRITE) ||
        flags != (MAP_PRIVATE | MAP_ANONYMOUS)) {
        errno = ENOTSUP;
        return MAP_FAILED;
    }
    if (!size || size > SIZE_MAX - 4095) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    size = (size + 4095) & ~(size_t)4095;
    Hl3Mapping* record = (Hl3Mapping*)malloc(sizeof(*record));
    if (!record) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    void* p = memalign(4096, size);
    if (!p) {
        free(record);
        errno = ENOMEM;
        return MAP_FAILED;
    }
    record->address = p;
    record->size = size;
    record->next = g_mappings;
    g_mappings = record;
    memset(p, 0, size);
    return p;
}

int mprotect(void* addr, size_t size, int prot) {
    (void)addr;
    (void)size;
    (void)prot;
    errno = ENOTSUP;
    return -1;
}

int munmap(void* addr, size_t size) {
    if (!size || size > SIZE_MAX - 4095) {
        errno = EINVAL;
        return -1;
    }
    size = (size + 4095) & ~(size_t)4095;
    Hl3Mapping** at = &g_mappings;
    while (*at && (*at)->address != addr) {
        at = &(*at)->next;
    }
    if (!*at || (*at)->size != size) {
        errno = EINVAL;
        return -1;
    }
    Hl3Mapping* record = *at;
    *at = record->next;
    free(record->address);
    free(record);
    return 0;
}

