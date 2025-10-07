/*
 * Copyright (c) 2021 Micha LaQua <micha.laqua@gmail.com>
 *
 * Special thanks to Ingo Buerk (Airblader) for his work on the
 * awesome unclutter-xfixes project, upon which the XInput eventcode
 * is based on.
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>

#define error(...) fprintf(stderr, __VA_ARGS__)

static bool watch_slave_devices = true;

int main(int argc, const char* argv[]) {
    (void) argc;
    (void) argv;
    int xi_opcode = -1;
    Display *display;

    display = XOpenDisplay(NULL);
    if (!display) {
        error("Failed to connect to X server\n");
        return 1;
    }

    int event, error_num;
    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error_num)) {
        error("XInput extension not available\n");
        return 1;
    }

    int major = 2, minor = 2;
    if (XIQueryVersion(display, &major, &minor) != Success) {
        error("XI2 >= 2.2 required\n");
        return 1;
    }

    XIEventMask mask;
    unsigned char mask_bits[(XI_LASTEVENT + 7)/8];
    memset(mask_bits, 0, sizeof(mask_bits));

    mask.deviceid = watch_slave_devices ? XIAllDevices : XIAllMasterDevices;
    mask.mask_len = sizeof(mask_bits);
    mask.mask = mask_bits;
    XISetMask(mask_bits, watch_slave_devices ? XI_ButtonPress : XI_RawButtonPress);

    XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
    XFlush(display);

    error("Blocking new mouse paste actions from all %s devices\n",
          watch_slave_devices ? "slave" : "master");

    int xfd = XConnectionNumber(display);
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);

        if (select(xfd + 1, &fds, NULL, NULL, NULL) > 0) {
            XEvent xevent;
            while (XPending(display) > 0) {
                XNextEvent(display, &xevent);
                XGenericEventCookie *cookie = &xevent.xcookie;

                if (cookie->type != GenericEvent ||
                    cookie->extension != xi_opcode ||
                    !XGetEventData(display, cookie))
                    continue;

                const XIRawEvent *data = (const XIRawEvent *) cookie->data;

                if (data->detail == 2) {  // middle mouse button
                    XSetSelectionOwner(display, XA_PRIMARY, None, CurrentTime);
                    XStoreBytes(display, None, 0);
                    XSetSelectionOwner(display, XA_STRING, None, CurrentTime);
                    XSync(display, False);
                    error("Cleared primary selection and cut buffer\n");
                }

                XFreeEventData(display, cookie);
            }
        }
    }

    return 0;
}
