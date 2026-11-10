// Execute the real heap-backed adapter on the Linux CI host, not a kernel mock.
#include <sys/mman.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
int main(void){
    int flags=MAP_PRIVATE|MAP_ANONYMOUS;
    unsigned char *p=mmap(NULL,8193,PROT_READ|PROT_WRITE,flags,-1,0);
    assert(p!=MAP_FAILED && ((uintptr_t)p%4096)==0);
    for(size_t i=0;i<12288;i++){assert(p[i]==0);p[i]=(unsigned char)i;}
    assert(mprotect(p,4096,PROT_NONE)==-1 && errno==ENOTSUP);
    assert(mprotect(p,4096,PROT_EXEC)==-1 && errno==ENOTSUP);
    assert(munmap(p,4096)==-1 && errno==EINVAL);
    assert(munmap(p,8193)==0); // No failed kernel permission reset or leaked cache.
    assert(munmap(p,8193)==-1 && errno==EINVAL);
    assert(mmap(NULL,0,PROT_READ|PROT_WRITE,flags,-1,0)==MAP_FAILED);
    assert(mmap(NULL,SIZE_MAX,PROT_READ|PROT_WRITE,flags,-1,0)==MAP_FAILED);
    assert(mmap(NULL,4096,PROT_EXEC,flags,-1,0)==MAP_FAILED);
    assert(mmap(NULL,4096,PROT_READ|PROT_WRITE,flags|MAP_FIXED,-1,0)==MAP_FAILED);
    assert(mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0)==MAP_FAILED);
    for(int i=0;i<16;i++){
        p=mmap(NULL,32*1024*1024,PROT_READ|PROT_WRITE,flags,-1,0);
        assert(p!=MAP_FAILED);p[32*1024*1024-1]=42;
        assert(munmap(p,32*1024*1024)==0);
    }
    puts("PASS Switch TCI heap adapter: allocation, ownership, release and explicit unsupported protections");
    return 0;
}
