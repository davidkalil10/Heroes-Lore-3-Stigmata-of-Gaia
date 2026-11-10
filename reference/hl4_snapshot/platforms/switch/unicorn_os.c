// TCI data buffers only. Executable mappings are intentionally unsupported.
#include <sys/mman.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef struct Mapping {void *address;size_t size;struct Mapping *next;} Mapping;
static Mapping *mappings;
void *mmap(void *addr,size_t size,int prot,int flags,int fd,off_t offset){
    if(addr || fd!=-1 || offset || prot!=(PROT_READ|PROT_WRITE) || flags!=(MAP_PRIVATE|MAP_ANONYMOUS)) {errno=ENOTSUP;return MAP_FAILED;}
    if(!size || size>SIZE_MAX-4095){errno=EINVAL;return MAP_FAILED;}
    size=(size+4095)&~(size_t)4095;
    Mapping *record=malloc(sizeof(*record));if(!record){errno=ENOMEM;return MAP_FAILED;}
    void *p=memalign(4096,size);if(!p){free(record);errno=ENOMEM;return MAP_FAILED;}
    record->address=p;record->size=size;record->next=mappings;mappings=record;
    memset(p,0,size);return p;
}
int mprotect(void *addr,size_t size,int prot){
    // This heap-backed TCI adapter does not implement permission transitions.
    // The optional TCG guard is explicitly disabled by the guarded source patch.
    (void)addr;(void)size;(void)prot;errno=ENOTSUP;return -1;
}
int munmap(void *addr,size_t size){
    if(!size || size>SIZE_MAX-4095){errno=EINVAL;return -1;}
    size=(size+4095)&~(size_t)4095;
    Mapping **at=&mappings;
    while(*at && (*at)->address!=addr)at=&(*at)->next;
    if(!*at || (*at)->size!=size){errno=EINVAL;return -1;}
    Mapping *record=*at;*at=record->next;free(record->address);free(record);return 0;
}
