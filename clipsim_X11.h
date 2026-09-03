#if !defined(CLIPSIM_X11_H)
#include "cbase.h"

#if CC_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#pragma clang diagnostic ignored "-Wreserved-identifier"

#endif

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#if CC_CLANG
#pragma clang diagnostic pop
#endif

#endif /* CLIPSIM_X11_H */
