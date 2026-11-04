/**
 * @file fh/platform.hpp
 * @brief Selects the platform specific file handle backend
 */

#ifndef LOCHFOLK_FH_PLATFORM_HPP
#define LOCHFOLK_FH_PLATFORM_HPP

#pragma once

// Windows always uses the Win32 API (CreateFileW/ReadFile/WriteFile/...).
// Unix-like platforms use the POSIX backend.
#if defined(_WIN32)
#    define LOCHFOLK_FH_WIN32 1
#elif defined(__unix__) || defined(__APPLE__)
#    define LOCHFOLK_FH_POSIX 1
#else
#    error "lochfolk: no file handle backend for this platform"
#endif

#endif
