// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(CBASE_RYU_H)
#define CBASE_RYU_H

#include "ryu/ryu.h"
#include "ryu/ryu_parse.h"

#endif /* CBASE_RYU_H */

#if defined(RYU_IMPLEMENT) && !defined(RYU_IMPLEMENTED)
#define RYU_IMPLEMENTED 1

#define to_chars ryu_d2s_to_chars
#include "ryu/d2s.c"
#undef to_chars

#define to_chars ryu_f2s_to_chars
#include "ryu/f2s.c"
#undef to_chars

#include "ryu/d2fixed.c"

#if CC_GCC || CC_CLANG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
#endif
#include "ryu/s2d.c"
#if CC_GCC || CC_CLANG
#pragma GCC diagnostic pop
#endif

#endif /* RYU_IMPLEMENT */
