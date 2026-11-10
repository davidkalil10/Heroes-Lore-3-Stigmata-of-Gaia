"""Small, reproducible Horizon/TCI adaptations to the pinned external source."""
import sys
from pathlib import Path
p=Path(sys.argv[1])
def patch(file,old,new):
    path=p/file;s=path.read_text()
    if new in s:return
    if old not in s:raise ValueError('Unicorn patch guard: '+file)
    path.write_text(s.replace(old,new,1))
patch('qemu/configure','if check_define __linux__ ; then','if check_define __SWITCH__ ; then\n  targetos="Horizon"\nelif check_define __linux__ ; then')
patch('qemu/configure','case $targetos in\nMINGW32*)','case $targetos in\nHorizon)\n  supported_os="yes"\n  QEMU_LDFLAGS="$QEMU_LDFLAGS -specs=/opt/devkitpro/libnx/switch.specs -L/opt/devkitpro/libnx/lib -lnx"\n  LIBS="-lnx $LIBS"\n;;\nMINGW32*)')
patch('CMakeLists.txt','if(UNICORN_FUZZ)\n        set(EXTRA_CFLAGS','if(UNICORN_FUZZ OR HL4_SWITCH)\n        set(EXTRA_CFLAGS')
patch('CMakeLists.txt','${UNICORN_EXECUTION_MODE}\n        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}\n    )','${UNICORN_EXECUTION_MODE}\n        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}\n        RESULT_VARIABLE HL4_CONFIG_RESULT\n    )\n    if(NOT HL4_CONFIG_RESULT EQUAL 0)\n        message(FATAL_ERROR "Unicorn configure failed; see config.log")\n    endif()')
# Horizon has no POSIX process signal masks. As in the upstream Win32 adapter,
# nonlocal CPU exception exits use setjmp/longjmp without saving a signal mask.
patch('qemu/include/qemu/osdep.h','#include <setjmp.h>','#include <setjmp.h>\n#ifdef __SWITCH__\n#define sigjmp_buf jmp_buf\n#define sigsetjmp(env, mask) setjmp(env)\n#define siglongjmp(env, value) longjmp(env, value)\n#endif')
patch('qemu/util/qemu-thread-posix.c','    pthread_sigmask(SIG_SETMASK, &set, &oldset);','#ifndef __SWITCH__\n    pthread_sigmask(SIG_SETMASK, &set, &oldset);\n#endif')
patch('qemu/util/qemu-thread-posix.c','    pthread_sigmask(SIG_SETMASK, &oldset, NULL);','#ifndef __SWITCH__\n    pthread_sigmask(SIG_SETMASK, &oldset, NULL);\n#endif')
patch('qemu/util/pagesize.c','uc->qemu_real_host_page_size = getpagesize();','#ifdef __SWITCH__\n    uc->qemu_real_host_page_size = 4096;\n#else\n    uc->qemu_real_host_page_size = getpagesize();\n#endif')
patch('qemu/util/oslib-posix.c','#include <uc_priv.h>','#include <uc_priv.h>\n#ifdef __SWITCH__\n#include <malloc.h>\n#endif')
patch('qemu/util/cacheinfo.c','#else\n/* POSIX */','#elif defined(__SWITCH__)\n/* Horizon: the real AArch64 CTR_EL0 probe below supplies cache geometry. */\nstatic void sys_cache_info(int *isize, int *dsize) { }\n#else\n/* POSIX */')
patch('glib_compat/grand.c','#include <math.h>','#include <math.h>\n#ifdef __SWITCH__\n#include <switch.h>\n#endif')
patch('glib_compat/grand.c','#if defined(unix) || defined(__unix__) || defined(__unix) || defined (__MINGW32__)','#if defined(__SWITCH__) || defined(unix) || defined(__unix__) || defined(__unix) || defined (__MINGW32__)')
patch('glib_compat/grand.c','guint32 seed[4];\n#if defined(unix)','guint32 seed[4];\n#if defined(__SWITCH__)\n    randomGet(seed, sizeof(seed));\n#elif defined(unix)')
# The memory interpreter does not need executable memory or fixed host maps.
patch('qemu/util/oslib-posix.c','{\n    int flags;\n    int map_sync_flags', '{\n#ifdef __SWITCH__\n    void *buffer = memalign(align < 4096 ? 4096 : align, size);\n    if (!buffer) return NULL;\n    memset(buffer, 0, size);\n    return buffer;\n#else\n    int flags;\n    int map_sync_flags')
# Exclude the remaining POSIX fixed-map implementation on Horizon.
patch('qemu/util/oslib-posix.c','    return ptr;\n}\n\nstatic void qemu_ram_munmap','    return ptr;\n    #endif\n}\n\nstatic void qemu_ram_munmap')
patch('qemu/util/oslib-posix.c','{\n    size_t pagesize;\n\n    if (ptr)', '{\n#ifdef __SWITCH__\n    free(ptr);return;\n#endif\n    size_t pagesize;\n\n    if (ptr)')
# Heap pages remain RW. TCG already bounds its usable region below this optional
# guard; never claim a permission change that Horizon's heap cannot provide.
patch('qemu/tcg/tcg.c','        (void)qemu_mprotect_none(end, page_size);','#if !defined(__SWITCH__)\n        (void)qemu_mprotect_none(end, page_size);\n#elif !CONFIG_TCG_INTERPRETER\n#error "HL4 Switch requires the data-only TCI backend"\n#endif')
print('Applied guarded Horizon/TCI source adaptations')
