#if !defined(CBASE_RYU_H)
#define CBASE_RYU_H

#include "ryu/ryu.h"

#if defined(CBASE_IMPLEMENT) && !defined(CBASE_RYU_IMPLEMENTED)
#define CBASE_RYU_IMPLEMENTED 1

#define to_chars ryu_d2s_to_chars
#include "ryu/d2s.c"
#undef to_chars

#define to_chars ryu_f2s_to_chars
#include "ryu/f2s.c"
#undef to_chars

#include "ryu/d2fixed.c"

#endif /* defined(CBASE_IMPLEMENT) && !defined(CBASE_RYU_IMPLEMENTED) */

#endif /* CBASE_RYU_H */
