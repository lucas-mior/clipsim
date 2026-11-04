#include "../cbase/cbase.h"

#if CC_CLANG || CC_GCC
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

int
main(void) {
    Display *d;
    Window w;
    Atom clip;

    if ((d = XOpenDisplay(NULL)) == NULL) {
        return 1;
    }

    w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 1, 1, 0, 0, 0);
    clip = XInternAtom(d, "CLIPBOARD", False);

    XSetSelectionOwner(d, clip, w, CurrentTime);
    XFlush(d);
    pause();

    return 0;
}
