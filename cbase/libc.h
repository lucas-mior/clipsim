// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

// Note: all libc or main platform headers must be included here.
// other files include them by `#include "cbase.h"` or `#include "libc.h"`
// Avoid including system headers in other files.

#if !defined(LIBC_H)
#define LIBC_H

#include "platform_detection.h"

#if CC_CLANG
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wreserved-identifier"
#endif

#if OS_UNIX && !defined(_XOPEN_SOURCE)
  #define _XOPEN_SOURCE 700
#endif

#if OS_UNIX && !defined(_DEFAULT_SOURCE)
  #define _DEFAULT_SOURCE
#endif

#if OS_LINUX && !defined(_GNU_SOURCE)
  #define _GNU_SOURCE
#endif

#if OS_NETBSD && !defined(_NETBSD_SOURCE)
  #define _NETBSD_SOURCE
#endif

#if CC_CLANG
  #pragma clang diagnostic pop
#endif

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <ftw.h>
#include <getopt.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#if OS_WINDOWS
#include <windows.h>
#endif

#if OS_UNIX
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <utime.h>
#endif

#if !defined(CBASE_HAS_FTS)
#if OS_UNIX
  #if defined(__has_include)
    #if __has_include(<fts.h>)
      #define CBASE_HAS_FTS 1
    #else
      #define CBASE_HAS_FTS 0
    #endif
  #elif defined(__GLIBC__) || OS_MAC || OS_BSD
    #define CBASE_HAS_FTS 1
  #else
    #define CBASE_HAS_FTS 0
  #endif
#else
  #define CBASE_HAS_FTS 0
#endif
#endif

#if CBASE_HAS_FTS
#include <fts.h>
#endif

#if OS_MAC || OS_BSD
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#if OS_LINUX
#include <sys/syscall.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#if !defined(CBASE_DIRENT_HAS_D_TYPE)
#if defined(DT_DIR)
#define CBASE_DIRENT_HAS_D_TYPE 1
#else
#define CBASE_DIRENT_HAS_D_TYPE 0
#endif
#endif

#if !defined(CBASE_HAS_F_GETPATH)
#if defined(F_GETPATH)
#define CBASE_HAS_F_GETPATH 1
#else
#define CBASE_HAS_F_GETPATH 0
#endif
#endif

#if !defined(CBASE_HAS_SYSTEM_MEMMEM)
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
#define CBASE_HAS_SYSTEM_MEMMEM 1
#else
#define CBASE_HAS_SYSTEM_MEMMEM 0
#endif
#endif

#if !defined(FLAGS_HUGE_PAGES)
#if defined(MAP_HUGETLB) && defined(MAP_HUGE_2MB)
#define FLAGS_HUGE_PAGES (MAP_HUGETLB | MAP_HUGE_2MB)
#else
#define FLAGS_HUGE_PAGES 0
#endif
#endif

#if !defined(MAP_POPULATE)
#define MAP_POPULATE 0
#endif

#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
#define MAP_ANON MAP_ANONYMOUS
#elif !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#elif !defined(MAP_ANONYMOUS) && !defined(MAP_ANON)
#define MAP_ANON 0
#define MAP_ANONYMOUS 0
#endif

#endif /* LIBC_H */
