// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(PLATFORM_DETECTION_H)
#define PLATFORM_DETECTION_H

#if defined(__linux__)
  #define OS_LINUX 1
  #define OS_MAC 0
  #define OS_BSD 0
  #define OS_WINDOWS 0
  #define OS_WASM 0
#elif defined(__APPLE__) && defined(__MACH__)
  #define OS_LINUX 0
  #define OS_MAC 1
  #define OS_BSD 0
  #define OS_WINDOWS 0
  #define OS_WASM 0
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  #define OS_LINUX 0
  #define OS_MAC 0
  #define OS_BSD 1
  #define OS_WINDOWS 0
  #define OS_WASM 0
#elif defined(_WIN32) || defined(_WIN64)
  #define OS_LINUX 0
  #define OS_MAC 0
  #define OS_BSD 0
  #define OS_WINDOWS 1
  #define OS_WASM 0
#elif defined(__wasm__)
  #define OS_LINUX 0
  #define OS_MAC 0
  #define OS_BSD 0
  #define OS_WINDOWS 0
  #define OS_WASM 1
#else
  #define OS_LINUX 0
  #define OS_MAC 0
  #define OS_BSD 0
  #define OS_WINDOWS 0
  #define OS_WASM 0
#endif

#define OS_UNIX (OS_LINUX || OS_MAC || OS_BSD)

#if defined(__clang__)
  #define CC_GCC 0
  #define CC_CLANG 1
  #define CC_TCC 0
  #define CC_MSVC 0
#elif defined(__GNUC__)
  #define CC_GCC 1
  #define CC_CLANG 0
  #define CC_TCC 0
  #define CC_MSVC 0
#elif defined(__TINYC__)
  #define CC_GCC 0
  #define CC_CLANG 0
  #define CC_TCC 1
  #define CC_MSVC 0
#elif defined(_MSC_VER)
  #define CC_GCC 0
  #define CC_CLANG 0
  #define CC_TCC 0
  #define CC_MSVC 1
#else
  #define CC_GCC 0
  #define CC_CLANG 0
  #define CC_TCC 0
  #define CC_MSVC 0
#endif

#define CC_TOY !(CC_GCC || CC_CLANG || CC_TCC || CC_MSVC)

#if OS_WINDOWS
#include <windows.h>
#endif

#if OS_UNIX
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#endif

#if OS_MAC
#include <sys/param.h>
#undef MIN
#undef MAX
#endif

#if OS_WINDOWS
#define RW_TYPE uint
#else
#define RW_TYPE size_t
#endif

#endif /* PLATFORM_DETECTION_H */
