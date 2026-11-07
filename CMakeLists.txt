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

# ---------------------------------------------------------------------------
# minizip-ng (ZIP archive support)
# ---------------------------------------------------------------------------
include(FetchContent)

# Lochfolk only reads ZIP archives and never passes a password to minizip-ng,
# so the OpenSSL-backed crypto backend (and WinZip AES) is dead code for us.
# With MZ_OPENSSL=OFF and MZ_WZAES=OFF minizip-ng builds with
# -DMZ_ZIP_NO_CRYPTO and never looks for or links OpenSSL. This is safe for
# Lochfolk: it never calls any crypt/wzaes API, while traditional PKWARE
# encryption stays enabled. Only WinZip AES archives (which Lochfolk cannot
# open anyway) become unavailable, at the cost of not pulling in OpenSSL.
#
# Every optional codec Lochfolk does not use is disabled as well, so the
# resulting dependency set is minizip-ng + zlib (plus GoogleTest for tests).
set(MZ_OPENSSL OFF CACHE BOOL "Enable OpenSSL for encryption" FORCE)
set(MZ_WZAES OFF CACHE BOOL "Enable WinZIP AES encryption" FORCE)
set(MZ_LZMA OFF CACHE BOOL "Enable LZMA/XZ compression" FORCE)
set(MZ_BZIP2 OFF CACHE BOOL "Enable BZIP2 compression" FORCE)
set(MZ_ZSTD OFF CACHE BOOL "Enable ZSTD compression" FORCE)
set(MZ_PPMD OFF CACHE BOOL "Enable PPMd compression" FORCE)
set(MZ_LIBCOMP OFF CACHE BOOL "Enable Apple's compression framework" FORCE)
set(MZ_LIBBSD OFF CACHE BOOL "Use libbsd for random numbers" FORCE)
set(MZ_FORCE_FETCH_LIBS OFF CACHE BOOL "Always fetch third-party libraries" FORCE)
set(MZ_BUILD_TESTS OFF CACHE BOOL "Build minizip-ng command line tools" FORCE)
set(MZ_BUILD_UNIT_TESTS OFF CACHE BOOL "Build minizip-ng unit tests" FORCE)
set(MZ_BUILD_FUZZ_TESTS OFF CACHE BOOL "Build minizip-ng fuzzers" FORCE)
set(MZ_CODE_COVERAGE OFF CACHE BOOL "Build minizip-ng with coverage" FORCE)
set(MZ_SANITIZER OFF CACHE STRING "minizip-ng sanitizer" FORCE)

# minizip-ng is an implementation detail: keep its install() rules out of the
# Lochfolk install tree.
set(SKIP_INSTALL_ALL ON)

FetchContent_Declare(
    minizip_ng
    GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
    GIT_TAG 4.2.2
    GIT_SHALLOW TRUE
)

# Always build minizip-ng as a static library, even when Lochfolk itself is
# shared: it is linked into liblochfolk.so / lochfolk.dll and must not become a
# public runtime dependency. minizip-ng itself still uses CMake's
# BUILD_SHARED_LIBS, so force that OFF around the fetch.
set(_lochfolk_build_shared_libs ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(minizip_ng)
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

# Lochfolk includes minizip-ng headers as <minizip/...>, matching the layout
# of an installed minizip-ng. Recreate that layout for the fetched sources.
set(LOCHFOLK_MINIZIP_SHIM_INCLUDE
    ${CMAKE_CURRENT_BINARY_DIR}/third_party/minizip/include
)
file(MAKE_DIRECTORY ${LOCHFOLK_MINIZIP_SHIM_INCLUDE}/minizip)
file(COPY ${minizip_ng_SOURCE_DIR}/
    DESTINATION ${LOCHFOLK_MINIZIP_SHIM_INCLUDE}/minizip
    FILES_MATCHING PATTERN "*.h"
)

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
        ${LOCHFOLK_MINIZIP_SHIM_INCLUDE}
)

target_compile_definitions(lochfolk PRIVATE LOCHFOLK_BUILD)
if(lochfolk_shared)
    target_compile_definitions(lochfolk PUBLIC LOCHFOLK_SHARED)
endif()

target_compile_options(lochfolk PRIVATE ${LOCHFOLK_WARNING_OPTIONS})

set_target_properties(lochfolk PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_link_libraries(lochfolk PRIVATE minizip)

# Vendored static dependencies are merged into the static library, so an
# installed liblochfolk.a is usable without a separate minizip-ng package. On
# Linux and macOS zlib stays a system shared library that static consumers
# still have to link; on Windows minizip-ng fetches zlib statically and that
# archive is embedded as well.
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
        else()
            # Keep the embedded zlib symbols hidden as well.
            set_target_properties(${_bundle_zlib_target} PROPERTIES
                C_VISIBILITY_PRESET hidden
            )
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
