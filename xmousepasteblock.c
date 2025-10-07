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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>

#define error(...) fprintf(stderr, __VA_ARGS__)

#define BUTTON_MIDDLE_CODE 2

static bool watch_slave_devices = true;

int main(int argc, const char* argv[]) {
    int xi_opcode;
    int x_connection_fd;
    struct pollfd poll_files[1];
    Display *display;

    (void) argc;
    (void) argv;

    if ((display = XOpenDisplay(NULL)) == NULL) {
        error("Error connecting to X server.\n");
        exit(EXIT_FAILURE);
    }

    {
        int event;
        int error_num;
        if (!XQueryExtension(display,
                             "XInputExtension", &xi_opcode,
                             &event, &error_num)) {
            error("XInput extension not available.\n");
            exit(EXIT_FAILURE);
        }
    }

    {
        int major = 2;
        int minor = 2;
        if (XIQueryVersion(display, &major, &minor) != Success) {
            error("XI2 >= %d.%d required\n", major, minor);
            exit(EXIT_FAILURE);
        }
    }

    {
        XIEventMask mask;
        unsigned char mask_bits[(XI_LASTEVENT + 7)/8];
        memset(mask_bits, 0, sizeof(mask_bits));

        mask.deviceid = XIAllDevices;
        mask.mask_len = sizeof(mask_bits);
        mask.mask = mask_bits;
        XISetMask(mask_bits, XI_ButtonPress);

        XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
        XFlush(display);
    }

    error("Blocking new mouse paste actions from all %s devices\n",
          watch_slave_devices ? "slave" : "master");

    x_connection_fd = XConnectionNumber(display);

    poll_files[0].fd = x_connection_fd;
    poll_files[0].events = POLLIN;

    while (true) {
        int polled;
        if ((polled = poll(poll_files, 1, -1)) < 0) {
            error("Error polling: %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (poll_files[0].revents & POLLIN) {
            XEvent xevent;
            while (XPending(display) > 0) {
                XGenericEventCookie *cookie;
                const XIRawEvent *data;

                XNextEvent(display, &xevent);
                cookie = &xevent.xcookie;

                if (cookie->type != GenericEvent ||
                    cookie->extension != xi_opcode ||
                    !XGetEventData(display, cookie))
                    continue;

                data = cookie->data;

                if (data->detail == BUTTON_MIDDLE_CODE) {
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
}
