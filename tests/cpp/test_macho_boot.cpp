#include "../../src/core/runtime.h"

#include <cstdio>
#include <string>

int main() {
    hl3::Runtime rt;
    std::string err;
    if (!rt.init({}, {}, false, &err)) {
        std::fprintf(stderr, "Runtime init failed: %s\n", err.c_str());
        return 1;
    }

    const auto& img = rt.machine().image();
    std::printf("Loaded Mach-O: %zu segments, %zu imports, %zu ObjC classes, entry=%s\n",
                img.segments.size(), img.imports.size(), img.objc_classes.size(),
                hl3::hex32(img.entry_pc).c_str());

    if (img.objc_classes.size() != 22) {
        std::fprintf(stderr, "Expected 22 ObjC classes, got %zu\n", img.objc_classes.size());
        return 2;
    }

    rt.boot_to_app_main();
    if (!rt.has_pending_app_main()) {
        std::fprintf(stderr, "Boot did not reach UIApplicationMain\n");
        return 3;
    }
    const auto& pm = rt.pending_app_main();
    std::printf("Reached UIApplicationMain: principal='%s' delegate='%s'\n",
                pm.principal.c_str(), pm.delegate.c_str());

    if (pm.delegate != "AppDelegate") {
        std::fprintf(stderr, "Unexpected delegate: %s\n", pm.delegate.c_str());
        return 4;
    }

    rt.run_launch();
    std::printf("AppDelegate launch succeeded! unimplemented=%zu heap_used=%u\n",
                rt.machine().unimplemented_counts().size(), rt.machine().heap_used());
    for (const auto& [k, cnt] : rt.machine().unimplemented_counts()) {
        std::printf("  unimpl: %s x%llu\n", k.c_str(), static_cast<unsigned long long>(cnt));
    }
    return 0;
}

