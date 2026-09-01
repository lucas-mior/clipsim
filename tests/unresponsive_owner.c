#include "../cbase/cbase.h"

#if CC_CLANG || CC_GCC
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

int
main(void) {
    Display *d = XOpenDisplay(NULL);
    if (!d) {
        return 1;
    }

    Window w = XCreateSimpleWindow(d, DefaultRootWindow(d),
                                   0, 0, 1, 1, 0, 0, 0);
    Atom clip = XInternAtom(d, "CLIPBOARD", False);

    XSetSelectionOwner(d, clip, w, CurrentTime);
    XFlush(d);
    pause();

    return 0;
}
