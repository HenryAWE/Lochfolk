/**
 * @file fh/platform.hpp
 * @brief Selects the platform specific file handle backend
 */

#ifndef LOCHFOLK_FH_PLATFORM_HPP
#define LOCHFOLK_FH_PLATFORM_HPP

#pragma once

// Windows always uses the Win32 API (CreateFileW/ReadFile/WriteFile/...).
// Other platforms use the POSIX backend when available, or a portable stdio
// fallback. Define LOCHFOLK_FH_FORCE_STDIO to exercise the fallback on a
// platform that has a native backend.
#if defined(_WIN32)
#    define LOCHFOLK_FH_WIN32 1
#elif defined(LOCHFOLK_FH_FORCE_STDIO)
#    define LOCHFOLK_FH_STDIO 1
#elif defined(__unix__) || defined(__APPLE__)
#    define LOCHFOLK_FH_POSIX 1
#else
#    define LOCHFOLK_FH_STDIO 1
#endif

#endif
