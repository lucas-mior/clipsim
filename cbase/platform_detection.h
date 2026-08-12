// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

// this is completely self contained,
// it does not depend on any other cbase/files

#if !defined(PLATFORM_DETECTION_H)
#define PLATFORM_DETECTION_H

#define CC_GCC   0
#define CC_CLANG 0
#define CC_TCC   0
#define CC_MSVC  0

#if defined(__clang__)
  #undef CC_CLANG
  #define CC_CLANG 1
#elif defined(__GNUC__)
  #undef CC_GCC
  #define CC_GCC 1
#elif defined(__TINYC__)
  #undef CC_TCC
  #define CC_TCC 1
#elif defined(_MSC_VER)
  #undef CC_MSVC
  #define CC_MSVC 1
#endif

#define CC_TOY !(CC_GCC || CC_CLANG || CC_TCC || CC_MSVC)

#define OS_LINUX   0
#define OS_MAC     0
#define OS_FREEBSD 0
#define OS_NETBSD  0
#define OS_OPENBSD 0
#define OS_WINDOWS 0
#define OS_WASM    0

#if defined(__linux__)
  #undef OS_LINUX
  #define OS_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
  #undef OS_MAC
  #define OS_MAC 1
#elif defined(__FreeBSD__)
  #undef OS_FREEBSD
  #define OS_FREEBSD 1
#elif defined(__NetBSD__)
  #undef OS_NETBSD
  #define OS_NETBSD 1
#elif defined(__OpenBSD__)
  #undef OS_OPENBSD
  #define OS_OPENBSD 1
#elif defined(_WIN32) || defined(_WIN64)
  #undef OS_WINDOWS
  #define OS_WINDOWS 1
#elif defined(__wasm__)
  #undef OS_WASM
  #define OS_WASM 1
#endif

#define OS_BSD (OS_FREEBSD | OS_NETBSD | OS_OPENBSD)
#define OS_UNIX (OS_LINUX || OS_MAC || OS_BSD)

#if defined(_MSC_VER)
#define CBASE_CRT_MSVC 1
#else
#define CBASE_CRT_MSVC 0
#endif

#define HAS_POSIX_WIN_SUBSET (OS_UNIX || OS_WASM || (OS_WINDOWS && !CBASE_CRT_MSVC))

#if !defined(CBASE_HAS_DIRECT_H)
#define CBASE_HAS_DIRECT_H CBASE_CRT_MSVC
#endif

#if !defined(CBASE_HAS_IO_H)
#define CBASE_HAS_IO_H CBASE_CRT_MSVC
#endif

#if !defined(CBASE_HAS_PROCFS)
#define CBASE_HAS_PROCFS OS_LINUX
#endif

#if !defined(CBASE_HAS_GETTEXT)
#define CBASE_HAS_GETTEXT OS_LINUX
#endif

#if OS_WINDOWS
#define RW_TYPE unsigned int
#else
#define RW_TYPE size_t
#endif

#endif /* PLATFORM_DETECTION_H */
