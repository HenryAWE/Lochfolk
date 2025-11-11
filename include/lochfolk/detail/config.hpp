#ifndef LOCHFOLK_CONFIG_HPP
#define LOCHFOLK_CONFIG_HPP

#pragma once

#ifdef LOCHFOLK_SHARED
#    ifdef LOCHFOLK_BUILD
#        ifdef _MSC_VER
#            define LOCHFOLK_API __declspec(dllexport)
#        elif defined(__clang__) || defined(__GNUC__)
#            define LOCHFOLK_API __attribute__((visibility("default")))
#        endif
#    else
#        ifdef _MSC_VER
#            define LOCHFOLK_API __declspec(dllimport)
#        elif defined(__clang__) || defined(__GNUC__)
#            define LOCHFOLK_API
#        endif
#    endif
#else
#    define LOCHFOLK_API
#endif

#ifdef __unix__
#    include <unistd.h>
#    ifdef _POSIX_VERSION
#        define LOCHFOLK_IMPL_POSIX _POSIX_VERSION
#    endif
#endif

#endif
