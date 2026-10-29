/**
 * @file fh/platform.hpp
 * @brief Selects the platform specific file handle backend
 */

#ifndef LOCHFOLK_FH_PLATFORM_HPP
#define LOCHFOLK_FH_PLATFORM_HPP

#pragma once

// One (and only one) of LOCHFOLK_FH_WIN32, LOCHFOLK_FH_POSIX and
// LOCHFOLK_FH_STDIO is defined. Define LOCHFOLK_FH_FORCE_STDIO to use the
// portable stdio backend even on platforms with a native one.
#if !defined(LOCHFOLK_FH_WIN32) && !defined(LOCHFOLK_FH_POSIX) && \
    !defined(LOCHFOLK_FH_STDIO)
#    if defined(LOCHFOLK_FH_FORCE_STDIO)
#        define LOCHFOLK_FH_STDIO 1
#    elif defined(_WIN32)
#        define LOCHFOLK_FH_WIN32 1
#    elif defined(__unix__) || defined(__APPLE__)
#        define LOCHFOLK_FH_POSIX 1
#    else
#        define LOCHFOLK_FH_STDIO 1
#    endif
#endif

#endif
