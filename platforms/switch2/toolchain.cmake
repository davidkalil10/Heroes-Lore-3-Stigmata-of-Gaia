# Use official Switch 2 CPU flags, headers, portlibs and libounce linker specs.
include("/opt/devkitpro/libounce/cmake/Switch2.cmake")
# Existing data-only Horizon memory adapter for Unicorn TCI.
string(APPEND CMAKE_C_FLAGS_INIT " -I${CMAKE_CURRENT_LIST_DIR}/../switch/compat")
string(APPEND CMAKE_CXX_FLAGS_INIT " -I${CMAKE_CURRENT_LIST_DIR}/../switch/compat")
