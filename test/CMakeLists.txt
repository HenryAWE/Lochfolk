# GoogleTest: use the system package by default, or fetch 1.14.0 when the user
# asks for a self-contained build (-Dlochfolk_prefer_system_gtest=OFF).
if(lochfolk_prefer_system_gtest)
    find_package(GTest REQUIRED)
else()
    include(FetchContent)

    set(INSTALL_GTEST OFF CACHE BOOL "Do not install GoogleTest" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock" FORCE)
    if(MSVC)
        # Match the default MSVC runtime used by Lochfolk (/MD*).
        set(gtest_force_shared_crt ON CACHE BOOL "Use the shared CRT" FORCE)
    endif()

    # Build GoogleTest as a static library regardless of the surrounding
    # BUILD_SHARED_LIBS value, so the test executables never need a GoogleTest
    # runtime next to them.
    set(_lochfolk_build_shared_libs ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
    set(BUILD_SHARED_LIBS ${_lochfolk_build_shared_libs})
    unset(_lochfolk_build_shared_libs)
endif()

find_package(Python3 3.6 REQUIRED COMPONENTS Interpreter)

# ---------------------------------------------------------------------------
# Test fixtures
#
# test_vfs looks for its data relative to the working directory as
# "test_vfs_data/...". Recreate that layout inside the build tree.
# ---------------------------------------------------------------------------
set(LOCHFOLK_TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_vfs_data)
set(LOCHFOLK_TEST_STAMP ${CMAKE_CURRENT_BINARY_DIR}/test-data.stamp)

file(GLOB_RECURSE LOCHFOLK_TEST_ARCHIVE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/archive/*
)
file(GLOB_RECURSE LOCHFOLK_TEST_DIR_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/dir/*
)

add_custom_command(
    OUTPUT ${LOCHFOLK_TEST_STAMP}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${LOCHFOLK_TEST_DATA_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LOCHFOLK_TEST_DATA_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/example.txt
        ${LOCHFOLK_TEST_DATA_DIR}/example.txt
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/dir
        ${LOCHFOLK_TEST_DATA_DIR}/dir
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/script/make_archive.py
        ${LOCHFOLK_TEST_DATA_DIR}/ar.zip
        ${CMAKE_CURRENT_SOURCE_DIR}/archive
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/script/make_unsafe_archives.py
        ${LOCHFOLK_TEST_DATA_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${LOCHFOLK_TEST_STAMP}
    DEPENDS
        ${LOCHFOLK_TEST_ARCHIVE_FILES}
        ${LOCHFOLK_TEST_DIR_FILES}
        ${CMAKE_CURRENT_SOURCE_DIR}/example.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/script/make_archive.py
        ${CMAKE_CURRENT_SOURCE_DIR}/script/make_unsafe_archives.py
    COMMENT "Staging VFS unit-test fixtures"
    VERBATIM
)

add_custom_target(lochfolk_test_data ALL DEPENDS ${LOCHFOLK_TEST_STAMP})

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
add_executable(test_path test_path.cpp)
target_link_libraries(test_path PRIVATE Lochfolk::lochfolk GTest::gtest_main)
target_compile_options(test_path PRIVATE ${LOCHFOLK_WARNING_OPTIONS})
add_test(NAME test_path COMMAND test_path)

add_executable(test_vfs test_vfs.cpp)
target_link_libraries(test_vfs PRIVATE Lochfolk::lochfolk GTest::gtest_main)
target_compile_options(test_vfs PRIVATE ${LOCHFOLK_WARNING_OPTIONS})
add_dependencies(test_vfs lochfolk_test_data)
add_test(
    NAME test_vfs
    COMMAND test_vfs
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
