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
#include <ev.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>

#include "util.c"

static Display *display;
static int xi_opcode = -1;

void stub_cb(EV_P_ ev_io *w, int revents) {
    (void) w;
    (void) revents;
    return;
}

void check_cb(EV_P_ ev_check *w, int revents) {
    XEvent xevent;
    (void) w;
    (void) revents;

    while (XPending(display) > 0) {
        XNextEvent(display, &xevent);
        XGenericEventCookie *cookie = &xevent.xcookie;
        if (cookie->type != GenericEvent
            || cookie->extension !=  xi_opcode
            || !XGetEventData(display, cookie)) {
#ifdef DEBUG
            error("DEBUG: Dropping event of type %i", cookie->type);
#endif
            continue;
        }

        const XIRawEvent *data = (const XIRawEvent *) cookie->data;
#ifdef DEBUG
        error("DEBUG: Registered button press %i on device %i (source device %i)\n", data->detail, data->deviceid, data->sourceid);
#endif
        if (data->detail == 2) {
            /* Clear primary selection */
            XSetSelectionOwner(display, XA_PRIMARY, None, CurrentTime);

            /* Also clear deprecated cut buffer */
            XStoreBytes(display, None, 0);
            XSetSelectionOwner(display, XA_STRING, None, CurrentTime);

            XSync(display, False);
#ifdef DEBUG
            error("DEBUG: Primary selection and cut buffer cleared\n");
#endif
            return;
        }

        XFreeEventData(display, cookie);
    }
    return;
}

int main(int argc, const char* argv[]) {
    struct ev_loop *ev_loop = EV_DEFAULT;
    int watch_slave_devices = 0;
    int event;
    int error_num;
    char *XMPB_WATCH_SLAVE_DEVICES;

    (void) argc;
    (void) argv;

    if ((display = XOpenDisplay(NULL)) == NULL) {
        error("Error: Failed to connect to the X server\n");
        exit(EXIT_FAILURE);
    }

    if ((XMPB_WATCH_SLAVE_DEVICES = getenv("XMPB_WATCH_SLAVE_DEVICES"))) {
        for (char *c = XMPB_WATCH_SLAVE_DEVICES; *c; c += 1) {
           *c = *c > 0x40 && *c < 0x5b ? *c | 0x60 : *c;
        }
        if (strcmp(XMPB_WATCH_SLAVE_DEVICES, "1") == 0
            || strcmp(XMPB_WATCH_SLAVE_DEVICES, "true") == 0) {
            watch_slave_devices = 1;
        }
    }

    if (!XQueryExtension(display,
                         "XInputExtension", &xi_opcode, &event, &error_num)) {
        error("Error: XInput extension not available\n");
        exit(EXIT_FAILURE);
    }

    int major_op = 2;
    int minor_op = 2;
    int result = XIQueryVersion(display, &major_op, &minor_op);
    if (result == BadRequest) {
        error("Error: XI2 is not supported in a sufficient version (>=2.2 required).\n");
        exit(EXIT_FAILURE);
    } else if (result != Success) {
        error("Error: Failed to query XI2\n");
        exit(EXIT_FAILURE);
    }
    XIEventMask masks[1];

    {
        unsigned char mask_master[(XI_LASTEVENT + 7)/8];
        memset(mask_master, 0, sizeof(mask_master));
        masks[0].mask_len = sizeof(mask_master);
        masks[0].mask = mask_master;
        if (watch_slave_devices) {
            masks[0].deviceid = XIAllDevices;
            XISetMask(mask_master, XI_ButtonPress);
        } else {
            masks[0].deviceid = XIAllMasterDevices;
            XISetMask(mask_master, XI_RawButtonPress);
        }
    }

    XISelectEvents(display, DefaultRootWindow(display), masks, 1);
    XFlush(display);

    {
        struct ev_io *x_watcher;
        struct ev_check *x_check;

        x_watcher = util_calloc(1, sizeof(*x_watcher));
        ev_io_init(x_watcher, stub_cb, XConnectionNumber(display), EV_READ);
        ev_io_start(ev_loop, x_watcher);
    
        x_check = util_calloc(1, sizeof(*x_check));
        ev_check_init(x_check, check_cb);
        ev_check_start(ev_loop, x_check);

    }

    error("Blocking new mouse paste actions from all %s devices\n",
          watch_slave_devices ? "slave" : "master");
    ev_run(ev_loop, 0);

    exit(EXIT_SUCCESS);
}
