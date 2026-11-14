cmake_minimum_required(VERSION 3.16)
project(heroes_lore_3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HL3_SWITCH "Build for Nintendo Switch (devkitA64 + libnx + Unicorn TCI)" OFF)
option(HL3_TCI "Use Unicorn TCI interpreter backend" ${HL3_SWITCH})

# Unicorn 2 ARMv6 engine (static library, ARM target only for fast builds)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(UNICORN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(UNICORN_INSTALL OFF CACHE BOOL "" FORCE)
set(UNICORN_LEGACY_STATIC_ARCHIVE OFF CACHE BOOL "" FORCE)
set(UNICORN_ARCH "arm" CACHE STRING "" FORCE)
if(HL3_TCI OR HL3_SWITCH)
    set(UNICORN_INTERPRETER ON CACHE BOOL "" FORCE)
    set(HL3_UNICORN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unicorn-tci/unicorn-engine-sys-tci")
else()
    set(HL3_UNICORN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unicorn")
endif()
add_subdirectory(${HL3_UNICORN_DIR} unicorn-build)

if(WIN32 OR HL3_SWITCH)
    set(ZLIB_USE_STATIC_LIBS ON)
endif()
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
    set(SDL_HIDAPI_LIBUSB OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/SDL2 SDL2-build)
    set(HL3_SDL2_TARGET SDL2::SDL2)
elseif(HL3_SWITCH)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
    set(HL3_SDL2_TARGET PkgConfig::SDL2)
else()
    find_package(SDL2 CONFIG QUIET)
    if(WIN32 AND TARGET SDL2::SDL2-static)
        set(HL3_SDL2_TARGET SDL2::SDL2-static)
    elseif(TARGET SDL2::SDL2)
        set(HL3_SDL2_TARGET SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        set(HL3_SDL2_TARGET SDL2::SDL2-static)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        set(HL3_SDL2_TARGET PkgConfig::SDL2)
    endif()

    if(NOT WIN32)
        find_package(CURL QUIET)
        if(TARGET CURL::libcurl)
            set(HL3_CURL_TARGET CURL::libcurl)
        else()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
            set(HL3_CURL_TARGET PkgConfig::CURL)
        endif()
    endif()
endif()

add_library(hl3_core STATIC
    src/core/zip_archive.cpp
    src/core/json_mini.cpp
    src/core/bundle.cpp
    src/core/macho_loader.cpp
    src/core/machine.cpp
    src/core/objc.cpp
    src/core/runtime.cpp
    src/platform/display_config.cpp
    src/platform/i18n.cpp
    src/platform/net_http.cpp
    src/platform/png_codec.cpp
    src/platform/cloud_save.cpp
    src/platform/updater.cpp
    src/platform/ui_renderer.cpp
    src/platform/options_modal.cpp
    src/platform/input_controller.cpp
    src/hle/hle_libc.cpp
    src/hle/hle_foundation.cpp
    src/hle/hle_uikit.cpp
    src/hle/gl_bridge.cpp
    src/hle/hle_gl.cpp
    src/audio/audio_mixer.cpp
    src/audio/hle_audio.cpp
)

target_include_directories(hl3_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${HL3_UNICORN_DIR}/include
)

target_link_libraries(hl3_core PUBLIC
    unicorn
    ZLIB::ZLIB
    ${HL3_SDL2_TARGET}
    Threads::Threads
)

if(WIN32)
    target_compile_definitions(hl3_core PUBLIC NOMINMAX _CRT_SECURE_NO_WARNINGS SDL_MAIN_HANDLED)
    target_link_libraries(hl3_core PUBLIC opengl32 winhttp shell32 gdi32 ole32)
elseif(ANDROID)
    target_link_libraries(hl3_core PUBLIC log android)
elseif(HL3_SWITCH)
    target_compile_definitions(hl3_core PUBLIC SDL_MAIN_HANDLED)
    target_link_libraries(hl3_core PUBLIC
        PkgConfig::CURL
        PkgConfig::SDL2
        EGL
        glapi
        drm_nouveau
        mbedtls
        mbedx509
        mbedcrypto
        nx
        m
    )
else()
    target_compile_definitions(hl3_core PUBLIC SDL_MAIN_HANDLED)
    target_link_libraries(hl3_core PUBLIC ${HL3_CURL_TARGET} ${CMAKE_DL_LIBS})
endif()

function(hl3_configure_exe TARGET_NAME)
    target_link_libraries(${TARGET_NAME} PRIVATE hl3_core)
    if(MINGW)
        target_link_options(${TARGET_NAME} PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endfunction()

if(ANDROID)
    add_library(main SHARED src/main.cpp)
    target_link_libraries(main PRIVATE hl3_core)
else()
    set(HL3_APP_SOURCES src/main.cpp)
    if(WIN32)
        list(APPEND HL3_APP_SOURCES assets/hl3.rc)
    endif()

    add_executable(HeroesLore3 ${HL3_APP_SOURCES})
    hl3_configure_exe(HeroesLore3)
    if(HL3_SWITCH)
        target_sources(HeroesLore3 PRIVATE platforms/switch/unicorn_os.c)
        target_include_directories(HeroesLore3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/platforms/switch/compat)
        set_target_properties(HeroesLore3 PROPERTIES SUFFIX ".elf")
    endif()

    if(NOT HL3_SWITCH AND NOT CMAKE_CROSSCOMPILING)
        enable_testing()

        add_executable(test_macho_boot tests/cpp/test_macho_boot.cpp)
        hl3_configure_exe(test_macho_boot)
        add_test(NAME macho_boot COMMAND test_macho_boot)

        add_executable(test_headless_runtime tests/cpp/test_headless_runtime.cpp)
        hl3_configure_exe(test_headless_runtime)
        add_test(NAME headless_runtime COMMAND test_headless_runtime)

        add_executable(test_platform_services tests/cpp/test_platform_services.cpp)
        hl3_configure_exe(test_platform_services)
        add_test(NAME platform_services COMMAND test_platform_services)
    endif()
endif()
