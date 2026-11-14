#!/usr/bin/env python3
"""Apply idempotent Horizon/Switch + TCI 32 MiB budget patches to pinned unicorn-tci source."""

from __future__ import annotations

import argparse
from pathlib import Path

from patch_unicorn_common import patch_unicorn_crc32


def patch_once(root: Path, rel: str, old: str, new: str) -> bool:
    target = root / rel
    if not target.exists():
        raise FileNotFoundError(f"Missing expected Switch TCI file: {target}")
    text = target.read_text(encoding="utf-8")
    if new in text:
        return False
    if old not in text:
        raise ValueError(f"Unicorn Switch patch guard failed in {target}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    return True


REPLACEMENTS: tuple[tuple[str, str, str], ...] = (
    (
        "qemu/configure",
        "if check_define __linux__ ; then",
        'if check_define __SWITCH__ ; then\n  targetos="Horizon"\nelif check_define __linux__ ; then',
    ),
    (
        "qemu/configure",
        "case $targetos in\nMINGW32*)",
        'case $targetos in\nHorizon)\n  supported_os="yes"\n'
        '  QEMU_LDFLAGS="$QEMU_LDFLAGS -specs=/opt/devkitpro/libnx/switch.specs -L/opt/devkitpro/libnx/lib -lnx"\n'
        '  LIBS="-lnx $LIBS"\n;;\nMINGW32*)',
    ),
    (
        "CMakeLists.txt",
        "if(UNICORN_FUZZ)\n        set(EXTRA_CFLAGS",
        "if(UNICORN_FUZZ OR HL3_SWITCH)\n        set(EXTRA_CFLAGS",
    ),
    (
        "CMakeLists.txt",
        "${UNICORN_EXECUTION_MODE}\n        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}\n    )",
        '${UNICORN_EXECUTION_MODE}\n        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}\n'
        '        RESULT_VARIABLE HL3_CONFIG_RESULT\n    )\n'
        '    if(NOT HL3_CONFIG_RESULT EQUAL 0)\n'
        '        message(FATAL_ERROR "Unicorn configure failed; see config.log")\n'
        "    endif()",
    ),
    # Horizon lacks POSIX signal masks; use non-signal-mask jmp_buf as in Win32.
    (
        "qemu/include/qemu/osdep.h",
        "#include <setjmp.h>",
        "#include <setjmp.h>\n#ifdef __SWITCH__\n#define sigjmp_buf jmp_buf\n"
        "#define sigsetjmp(env, mask) setjmp(env)\n#define siglongjmp(env, value) longjmp(env, value)\n#endif",
    ),
    (
        "qemu/util/qemu-thread-posix.c",
        "    pthread_sigmask(SIG_SETMASK, &set, &oldset);",
        "#ifndef __SWITCH__\n    pthread_sigmask(SIG_SETMASK, &set, &oldset);\n#endif",
    ),
    (
        "qemu/util/qemu-thread-posix.c",
        "    pthread_sigmask(SIG_SETMASK, &oldset, NULL);",
        "#ifndef __SWITCH__\n    pthread_sigmask(SIG_SETMASK, &oldset, NULL);\n#endif",
    ),
    (
        "qemu/util/pagesize.c",
        "uc->qemu_real_host_page_size = getpagesize();",
        "#ifdef __SWITCH__\n    uc->qemu_real_host_page_size = 4096;\n"
        "#else\n    uc->qemu_real_host_page_size = getpagesize();\n#endif",
    ),
    (
        "qemu/util/oslib-posix.c",
        "#include <uc_priv.h>",
        "#include <uc_priv.h>\n#ifdef __SWITCH__\n#include <malloc.h>\n#endif",
    ),
    (
        "qemu/util/cacheinfo.c",
        "#else\n/* POSIX */",
        "#elif defined(__SWITCH__)\n"
        "/* Horizon: AArch64 CTR_EL0 handles cache line geometry. */\n"
        "static void sys_cache_info(int *isize, int *dsize) { }\n"
        "#else\n/* POSIX */",
    ),
    (
        "glib_compat/grand.c",
        "#include <math.h>",
        "#include <math.h>\n#ifdef __SWITCH__\n#include <switch.h>\n#endif",
    ),
    (
        "glib_compat/grand.c",
        "#if defined(unix) || defined(__unix__) || defined(__unix) || defined (__MINGW32__)",
        "#if defined(__SWITCH__) || defined(unix) || defined(__unix__) || defined(__unix) || defined (__MINGW32__)",
    ),
    (
        "glib_compat/grand.c",
        "guint32 seed[4];\n#if defined(unix)",
        "guint32 seed[4];\n#if defined(__SWITCH__)\n    randomGet(seed, sizeof(seed));\n#elif defined(unix)",
    ),
    (
        "qemu/util/oslib-posix.c",
        "{\n    int flags;\n    int map_sync_flags",
        "{\n#ifdef __SWITCH__\n    void *buffer = memalign(align < 4096 ? 4096 : align, size);\n"
        "    if (!buffer) return NULL;\n    memset(buffer, 0, size);\n    return buffer;\n#else\n"
        "    int flags;\n    int map_sync_flags",
    ),
    (
        "qemu/util/oslib-posix.c",
        "    return ptr;\n}\n\nstatic void qemu_ram_munmap",
        "    return ptr;\n    #endif\n}\n\nstatic void qemu_ram_munmap",
    ),
    (
        "qemu/util/oslib-posix.c",
        "{\n    size_t pagesize;\n\n    if (ptr)",
        "{\n#ifdef __SWITCH__\n    free(ptr);return;\n#endif\n    size_t pagesize;\n\n    if (ptr)",
    ),
    (
        "qemu/tcg/tcg.c",
        "        (void)qemu_mprotect_none(end, page_size);",
        "#if !defined(__SWITCH__)\n        (void)qemu_mprotect_none(end, page_size);\n"
        '#elif !CONFIG_TCG_INTERPRETER\n#error "HL3 Switch requires the data-only TCI backend"\n#endif',
    ),
    # Keep TCI codegen buffer within 32 MiB on Switch AArch64 instead of default 1 GiB.
    (
        "qemu/accel/tcg/translate-all.c",
        "#if TCG_TARGET_REG_BITS == 32\n#define DEFAULT_CODE_GEN_BUFFER_SIZE_1 (32 * MiB)",
        "#if TCG_TARGET_REG_BITS == 32 || defined(__SWITCH__)\n#define DEFAULT_CODE_GEN_BUFFER_SIZE_1 (32 * MiB)",
    ),
)


def patch_switch_unicorn(tci_engine_dir: Path) -> int:
    applied = patch_unicorn_crc32(tci_engine_dir)
    for rel, old, new in REPLACEMENTS:
        if patch_once(tci_engine_dir, rel, old, new):
            applied += 1
    return applied


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tci_engine_dir", type=Path, help="Path to unicorn-engine-sys-tci")
    args = parser.parse_args()
    patch_switch_unicorn(args.tci_engine_dir)
    print(f"OK: Applied guarded Horizon/TCI patches in {args.tci_engine_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
