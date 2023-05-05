/* This file is part of clipsim. */
/* Copyright (C) 2022 Lucas Mior */

/* This program is free software: you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or */
/* (at your option) any later version. */

/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>.*/

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>

#include "clipsim.h"

static Display *display;
static Atom CLIPBOARD, XSEL_DATA, INCR;
static Atom UTF8_STRING, image_png, TARGETS;
static XEvent xevent;
static Window window;

static Atom clipboard_check_target(Atom);
static int32 clipboard_get_clipboard(char **, ulong *);
static void clipboard_signal_program(void);

int clipboard_daemon_watch(void *unused) {
    DEBUG_PRINT("*clipboard_daemon_watch(void *unused) %d\n", __LINE__)
    ulong color;
    Window root;
    struct timespec pause;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;
    (void) unused;

    if (!(display = XOpenDisplay(NULL))) {
        fprintf(stderr, "Error opening X display.");
        exit(EXIT_FAILURE);
    }

    CLIPBOARD   = XInternAtom(display, "CLIPBOARD",   False);
    XSEL_DATA   = XInternAtom(display, "XSEL_DATA",   False);
    INCR        = XInternAtom(display, "INCR",        False);
    UTF8_STRING = XInternAtom(display, "UTF8_STRING", False);
    image_png   = XInternAtom(display, "image/png",   False);
    TARGETS     = XInternAtom(display, "TARGETS",     False);

    root = DefaultRootWindow(display);
    color = BlackPixel(display, DefaultScreen(display));
    window = XCreateSimpleWindow(display, root, 0,0, 1,1, 0, color, color);

    XFixesSelectSelectionInput(display, root, CLIPBOARD, (ulong)
                               XFixesSetSelectionOwnerNotifyMask
                             | XFixesSelectionClientCloseNotifyMask
                             | XFixesSelectionWindowDestroyNotifyMask);

    while (true) {
        char *save = NULL;
        ulong length;
        nanosleep(&pause, NULL);
        (void) XNextEvent(display, &xevent);
        mtx_lock(&lock);

        clipboard_signal_program();

        switch (clipboard_get_clipboard(&save, &length)) {
        case CLIPBOARD_TEXT:
            history_append(save, length);
            break;
        case CLIPBOARD_IMAGE:
            history_append(save, length);
            break;
        case CLIPBOARD_OTHER:
            fprintf(stderr, "Unsupported format. Clipsim only"
                            " works with UTF-8 and images.\n");
            break;
        case CLIPBOARD_LARGE:
            fprintf(stderr, "Buffer is too large and "
                            "INCR reading is not implemented yet. "
                            "This data won't be saved to history.\n");
            break;
        case CLIPBOARD_ERROR:
            history_recover(-1);
            break;
        }
        mtx_unlock(&lock);
    }
}

Atom clipboard_check_target(const Atom target) {
    DEBUG_PRINT("clipboard_check_target(%lu)\n", target)
    XEvent xev;

    XConvertSelection(display, CLIPBOARD, target, XSEL_DATA,
                      window, CurrentTime);
    do {
        (void) XNextEvent(display, &xev);
    } while (xev.type != SelectionNotify
          || xev.xselection.selection != CLIPBOARD);

    return xev.xselection.property;
}

int32 clipboard_get_clipboard(char **save, ulong *length) {
    DEBUG_PRINT("clipboard_get_clipboard(%p, %lu)\n", (void *) save, *length)
    int actual_format_return;
    ulong nitems_return;
    ulong bytes_after_return;
    Atom return_atom;

    if (clipboard_check_target(UTF8_STRING)) {
        XGetWindowProperty(display, window, XSEL_DATA, 0, LONG_MAX/4,
                           False, AnyPropertyType, &return_atom,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **) save);
        if (return_atom == INCR) {
            return CLIPBOARD_LARGE;
        } else {
            *length = nitems_return;
            return CLIPBOARD_TEXT;
        }
    } else if (clipboard_check_target(image_png)) {
        XGetWindowProperty(display, window, XSEL_DATA, 0, LONG_MAX/4,
                           False, AnyPropertyType, &return_atom,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **) save);
        if (return_atom == INCR) {
            return CLIPBOARD_LARGE;
        } else {
            *length = nitems_return;
            return CLIPBOARD_IMAGE;
        }
    } else if (clipboard_check_target(TARGETS)) {
        return CLIPBOARD_OTHER;
    }
    return CLIPBOARD_ERROR;
}

void clipboard_signal_program(void) {
    DEBUG_PRINT("clipboard_signal_program(void) %d\n", __LINE__)
    int signal_number;
    char *CLIPSIM_SIGNAL_CODE;
    char *CLIPSIM_SIGNAL_PROGRAM;

    if (!(CLIPSIM_SIGNAL_CODE = getenv("CLIPSIM_SIGNAL_CODE")))
        return;

    if (!(CLIPSIM_SIGNAL_PROGRAM = getenv("CLIPSIM_SIGNAL_PROGRAM")))
        return;

    if ((signal_number = atoi(CLIPSIM_SIGNAL_CODE)) < 10) {
        fprintf(stderr, "Invalid CLIPSIM_SIGNAL_CODE environment "
                        "variable: %s\n", CLIPSIM_SIGNAL_CODE);
        return;
    }

    send_signal(CLIPSIM_SIGNAL_PROGRAM, signal_number);
}
