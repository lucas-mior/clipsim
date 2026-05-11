#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clipsim.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int is_large = atoi(argv[1]);
    Display *d = XOpenDisplay(NULL);
    if (!d) return 1;
    Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 1, 1, 0, 0, 0);
    Atom clip = XInternAtom(d, "CLIPBOARD", False);
    Atom targets = XInternAtom(d, "TARGETS", False);
    Atom utf8 = XInternAtom(d, "UTF8_STRING", False);
    Atom incr = XInternAtom(d, "INCR", False);

    XSetSelectionOwner(d, clip, w, CurrentTime);
    XEvent e;
    while (1) {
        XNextEvent(d, &e);
        if (e.type == SelectionRequest) {
            XSelectionRequestEvent *req = &e.xselectionrequest;
            XEvent res;
            res.xselection.type = SelectionNotify;
            res.xselection.requestor = req->requestor;
            res.xselection.selection = req->selection;
            res.xselection.target = req->target;
            res.xselection.time = req->time;
            
            if (req->target == targets) {
                Atom t[] = { targets, utf8 };
                XChangeProperty(d, req->requestor, req->property, XA_ATOM, 32, PropModeReplace, (unsigned char*)t, 2);
                res.xselection.property = req->property;
                XSendEvent(d, req->requestor, False, 0, &res);
            } else if (req->target == utf8) {
                long min_size = 1;
                XChangeProperty(d, req->requestor, req->property, incr, 32, PropModeReplace, (unsigned char*)&min_size, 1);
                res.xselection.property = req->property;
                XSelectInput(d, req->requestor, PropertyChangeMask);
                XSendEvent(d, req->requestor, False, 0, &res);
                
                XEvent pe;
                while (1) {
                    XNextEvent(d, &pe);
                    if (pe.type == PropertyNotify && pe.xproperty.state == PropertyDelete && pe.xproperty.atom == req->property) break;
                }
                
                if (!is_large) {
                    char *chunk1 = "small_";
                    XChangeProperty(d, req->requestor, req->property, utf8, 8, PropModeReplace, (unsigned char*)chunk1, 6);
                    while (1) {
                        XNextEvent(d, &pe);
                        if (pe.type == PropertyNotify && pe.xproperty.state == PropertyDelete && pe.xproperty.atom == req->property) break;
                    }
                    char *chunk2 = "incr_test";
                    XChangeProperty(d, req->requestor, req->property, utf8, 8, PropModeReplace, (unsigned char*)chunk2, 9);
                    while (1) {
                        XNextEvent(d, &pe);
                        if (pe.type == PropertyNotify && pe.xproperty.state == PropertyDelete && pe.xproperty.atom == req->property) break;
                    }
                } else {
                    int size = ENTRY_MAX_LENGTH * 2;
                    char *buf = malloc2(size);
                    memset(buf, 'A', size);
                    XChangeProperty(d, req->requestor, req->property, utf8, 8, PropModeReplace, (unsigned char*)buf, size);
                    free2(buf, size);
                    while (1) {
                        XNextEvent(d, &pe);
                        if (pe.type == PropertyNotify && pe.xproperty.state == PropertyDelete && pe.xproperty.atom == req->property) break;
                    }
                }
                
                XChangeProperty(d, req->requestor, req->property, utf8, 8, PropModeReplace, NULL, 0);
                XFlush(d);
                break; 
            }
        }
    }
    XFlush(d);
    sleep(1);
    XCloseDisplay(d);
    return 0;
}
