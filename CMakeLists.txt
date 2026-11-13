cmake_minimum_required(VERSION 3.20)

project(
    Lochfolk
    VERSION 0.3.0
    DESCRIPTION "Yet another C++ virtual filesystem library"
    LANGUAGES C CXX
)

# The previous build system defaulted to a release build; keep that behaviour
# for the common single-configuration generators (Ninja, Makefiles, ...).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel
    )
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

# Keep runtime binaries (the Windows DLL and the test executables) in one
# directory, so that a shared Windows build can load lochfolk.dll next to the
# test executables.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(GNUInstallDirs)

option(lochfolk_shared "Build Lochfolk as a shared library" OFF)
option(lochfolk_enable_lto
    "Enable link-time optimization (default: ON for shared builds)" ${lochfolk_shared}
)
option(lochfolk_build_test "Build the Lochfolk unit tests" OFF)
option(lochfolk_prefer_system_gtest
    "Use the system GoogleTest instead of fetching version 1.14.0" ON
)
option(lochfolk_enable_asan
    "Build with AddressSanitizer (GCC/Clang only)" OFF
)

if(lochfolk_build_test)
    enable_testing()
endif()

if(lochfolk_enable_asan)
    if(MSVC)
        message(FATAL_ERROR "lochfolk_enable_asan is not supported with MSVC")
    endif()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(MSVC)
    set(LOCHFOLK_WARNING_OPTIONS /W3 /WX)
else()
    set(LOCHFOLK_WARNING_OPTIONS -Wall -Werror)
endif()

# Shared builds enable LTO by default. Fall back to a normal build when the
# toolchain does not support it, so exotic compilers still configure.
set(LOCHFOLK_LTO_ENABLED OFF)
if(lochfolk_enable_lto)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _lochfolk_ipo_supported OUTPUT _lochfolk_ipo_output)
    if(_lochfolk_ipo_supported)
        set(LOCHFOLK_LTO_ENABLED ON)
    else()
        message(WARNING
            "LTO was requested but is not supported by the toolchain; "
            "building without LTO.\n${_lochfolk_ipo_output}"
        )
    endif()
    unset(_lochfolk_ipo_supported)
    unset(_lochfolk_ipo_output)
endif()

# ---------------------------------------------------------------------------
# minizip-ng (ZIP archive support)
# ---------------------------------------------------------------------------
# minizip-ng is vendored as a git submodule at third_party/minizip-ng.
set(LOCHFOLK_MINIZIP_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minizip-ng
)
if(NOT EXISTS ${LOCHFOLK_MINIZIP_SOURCE_DIR}/CMakeLists.txt)
    message(FATAL_ERROR
        "third_party/minizip-ng is missing. Initialize the submodules with:"
        "\n    git submodule update --init --recursive"
    )
endif()

# minizip-ng resolves its compression backend at configure time: it prefers
# zlib-ng (native API) over classic zlib and otherwise fetches zlib-ng from
# GitHub. Lochfolk vendors the pinned zlib-ng as a submodule instead, so every
# platform builds the same version offline. The minizip-ng options below make
# it take its fetch path and point that path at the submodule.
set(LOCHFOLK_ZLIB_NG_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-ng
)
if(NOT EXISTS ${LOCHFOLK_ZLIB_NG_SOURCE_DIR}/CMakeLists.txt)
    message(FATAL_ERROR
        "third_party/zlib-ng is missing. Initialize the submodules with:"
        "\n    git submodule update --init --recursive"
    )
endif()

# Lochfolk only reads ZIP archives and never passes a password to minizip-ng,
# so the OpenSSL-backed crypto backend (and WinZip AES) is dead code for us.
# With MZ_OPENSSL=OFF and MZ_WZAES=OFF minizip-ng builds with
# -DMZ_ZIP_NO_CRYPTO and never looks for or links OpenSSL. This is safe for
# Lochfolk: it never calls any crypt/wzaes API, while traditional PKWARE
# encryption stays enabled. Only WinZip AES archives (which Lochfolk cannot
# open anyway) become unavailable, at the cost of not pulling in OpenSSL.
#
# Every optional codec Lochfolk does not use is disabled as well, so the
# resulting dependency set is minizip-ng + zlib-ng (plus GoogleTest for
# tests).
set(MZ_OPENSSL OFF CACHE BOOL "Enable OpenSSL for encryption" FORCE)
set(MZ_WZAES OFF CACHE BOOL "Enable WinZIP AES encryption" FORCE)
set(MZ_LZMA OFF CACHE BOOL "Enable LZMA/XZ compression" FORCE)
set(MZ_BZIP2 OFF CACHE BOOL "Enable BZIP2 compression" FORCE)
set(MZ_ZSTD OFF CACHE BOOL "Enable ZSTD compression" FORCE)
set(MZ_PPMD OFF CACHE BOOL "Enable PPMd compression" FORCE)
set(MZ_LIBCOMP OFF CACHE BOOL "Enable Apple's compression framework" FORCE)
set(MZ_LIBBSD OFF CACHE BOOL "Use libbsd for random numbers" FORCE)

# Do not let minizip-ng probe the system for zlib or zlib-ng: force its fetch
# path and redirect that path to the vendored zlib-ng tree. MZ_FETCH_LIBS is
# required in addition to MZ_FORCE_FETCH_LIBS because it defaults to OFF
# outside Windows and gates the fetch branch; without it minizip-ng would
# build without zlib support at all.
set(MZ_FETCH_LIBS ON CACHE BOOL "Fetch third-party libraries" FORCE)
set(MZ_FORCE_FETCH_LIBS ON CACHE BOOL "Always fetch third-party libraries" FORCE)
set(FETCHCONTENT_SOURCE_DIR_ZLIB ${LOCHFOLK_ZLIB_NG_SOURCE_DIR}
    CACHE PATH "Vendored zlib-ng source tree used by minizip-ng" FORCE
)

set(MZ_BUILD_TESTS OFF CACHE BOOL "Build minizip-ng command line tools" FORCE)
set(MZ_BUILD_UNIT_TESTS OFF CACHE BOOL "Build minizip-ng unit tests" FORCE)
set(MZ_BUILD_FUZZ_TESTS OFF CACHE BOOL "Build minizip-ng fuzzers" FORCE)
set(MZ_CODE_COVERAGE OFF CACHE BOOL "Build minizip-ng with coverage" FORCE)
set(MZ_SANITIZER OFF CACHE STRING "minizip-ng sanitizer" FORCE)

# minizip-ng is an implementation detail: keep its install() rules out of the
# Lochfolk install tree.
set(SKIP_INSTALL_ALL ON)

# Always build minizip-ng as a static library, even when Lochfolk itself is
# shared: it is linked into liblochfolk.so / lochfolk.dll and must not become a
# public runtime dependency. minizip-ng itself still uses CMake's
# BUILD_SHARED_LIBS, so force that OFF around the add_subdirectory().
set(_lochfolk_build_shared_libs ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(
    ${LOCHFOLK_MINIZIP_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/third_party/minizip-ng
)
set(BUILD_SHARED_LIBS ${_lochfolk_build_shared_libs})
unset(_lochfolk_build_shared_libs)

# minizip-ng is an implementation detail: build it with hidden visibility so
# its symbols stay out of the dynamic symbol table of liblochfolk.so and of any
# shared library that links the bundled static archive. PIC lets the static
# objects be linked into a shared Lochfolk.
set_target_properties(minizip PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
)

# zlib-ng is an implementation detail as well. PIC lets its static objects be
# linked into liblochfolk.so (shared builds) and into downstream shared
# libraries that embed liblochfolk.a; hidden visibility keeps the zng_*
# symbols out of the dynamic symbol table. minizip-ng does not set either
# itself because it normally links a system zlib.
if(TARGET zlib-ng)
    set_target_properties(zlib-ng PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
    )
else()
    message(FATAL_ERROR
        "minizip-ng did not configure the vendored zlib-ng target. Expected "
        "FETCHCONTENT_SOURCE_DIR_ZLIB to be honoured."
    )
endif()

# ---------------------------------------------------------------------------
# Lochfolk
# ---------------------------------------------------------------------------
if(lochfolk_shared)
    set(LOCHFOLK_LIBRARY_TYPE SHARED)
else()
    set(LOCHFOLK_LIBRARY_TYPE STATIC)
endif()

add_library(lochfolk ${LOCHFOLK_LIBRARY_TYPE}
    src/archive.cpp
    src/errmsg.cpp
    src/file_node.cpp
    src/io.cpp
    src/lochfolk.cpp
    src/path.cpp
    src/stream.cpp
    src/vfs.cpp
    src/fh/crlf.cpp
    src/fh/file_posix.cpp
    src/fh/file_win32.cpp
    src/fh/streambuf.cpp
)
add_library(Lochfolk::lochfolk ALIAS lochfolk)

target_compile_features(lochfolk PUBLIC cxx_std_20)

target_include_directories(lochfolk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(lochfolk PRIVATE LOCHFOLK_BUILD)
if(lochfolk_shared)
    target_compile_definitions(lochfolk PUBLIC LOCHFOLK_SHARED)
endif()

target_compile_options(lochfolk PRIVATE ${LOCHFOLK_WARNING_OPTIONS})

set_target_properties(lochfolk PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    INTERPROCEDURAL_OPTIMIZATION ${LOCHFOLK_LTO_ENABLED}
)

# The minizip target publishes the vendored source tree and its generated
# mz_config.h as build-interface include directories; Lochfolk includes the
# headers directly (<mz.h>, <mz_zip.h>, ...), so no installed-layout copy is
# needed.
target_link_libraries(lochfolk PRIVATE minizip)

# Vendored static dependencies are merged into the static library, so an
# installed liblochfolk.a is self-contained: consumers need neither a
# minizip-ng package nor zlib/zlib-ng. Shared builds embed the same static
# archives in liblochfolk.so / lochfolk.dll, so they have no zlib runtime
# dependency either.
if(NOT lochfolk_shared)
    set(_bundle_zlib_target)
    if(TARGET zlib-ng)
        set(_bundle_zlib_target zlib-ng)
    elseif(TARGET zlibstatic)
        set(_bundle_zlib_target zlibstatic)
    endif()
    if(_bundle_zlib_target)
        get_target_property(_bundle_zlib_type ${_bundle_zlib_target} TYPE)
        if(NOT _bundle_zlib_type STREQUAL "STATIC_LIBRARY")
            set(_bundle_zlib_target)
        endif()
    endif()

    set(_bundle_args
        "-DLOCHFOLK_OUTPUT=$<TARGET_FILE:lochfolk>"
        "-DLOCHFOLK_MINIZIP=$<TARGET_FILE:minizip>"
        "-DLOCHFOLK_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_static"
        "-DLOCHFOLK_AR=${CMAKE_AR}"
        "-DLOCHFOLK_RANLIB=${CMAKE_RANLIB}"
    )
    if(MSVC)
        list(APPEND _bundle_args "-DLOCHFOLK_MSVC=TRUE")
    endif()
    if(_bundle_zlib_target)
        list(APPEND _bundle_args
            "-DLOCHFOLK_ZLIB=$<TARGET_FILE:${_bundle_zlib_target}>"
        )
        add_dependencies(lochfolk ${_bundle_zlib_target})
    endif()

    add_custom_command(
        TARGET lochfolk
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} ${_bundle_args}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bundle_static_archive.cmake"
        COMMENT "Bundling minizip-ng into the static library"
        VERBATIM
    )
endif()

if(lochfolk_build_test)
    add_subdirectory(test)
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
# Install rules are only added when Lochfolk is the top-level project, so that
# including it with add_subdirectory() does not pollute the parent's install
# tree.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(
        TARGETS lochfolk
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()
