/* This file is part of clipsim.
 * Copyright (C) 2024 Lucas Mior

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include "clipsim.h"
#define CHECK_TARGET_MAX_EVENTS 100

static Display *display;
static Atom CLIPBOARD, XSEL_DATA, INCR;
static Atom UTF8_STRING, image_png, TARGETS;
static Window window;

static Atom clipboard_check_target(Atom);
static int32 clipboard_get_clipboard(char **, ulong *);

int
clipboard_daemon_watch(void) {
    DEBUG_PRINT("void");
    ulong color;
    Window root;
    struct timespec pause;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;
    char *CLIPSIM_SIGNAL_NUMBER;
    char *CLIPSIM_SIGNAL_PROGRAM;

    if ((display = XOpenDisplay(NULL)) == NULL) {
        error("Error opening X display.");
        exit(EXIT_FAILURE);
    }

    content_initialize_magic();

    int signal_number = 0;
    if ((CLIPSIM_SIGNAL_PROGRAM = getenv("CLIPSIM_SIGNAL_PROGRAM")) == NULL)
        error("CLIPSIM_SIGNAL_PROGRAM is not defined.\n");
    if ((CLIPSIM_SIGNAL_NUMBER = getenv("CLIPSIM_SIGNAL_NUMBER")) == NULL)
        error("CLIPSIM_SIGNAL_NUMBER is not defined.\n");
    if (CLIPSIM_SIGNAL_PROGRAM && CLIPSIM_SIGNAL_NUMBER) {
        if ((signal_number = atoi(CLIPSIM_SIGNAL_NUMBER)) <= 0) {
            error("Invalid CLIPSIM_SIGNAL_NUMBER environment variable: %s.\n",
                  CLIPSIM_SIGNAL_NUMBER);
            if (CLIPSIM_SIGNAL_PROGRAM)
                error("%s will not be signaled.\n", CLIPSIM_SIGNAL_PROGRAM);

            CLIPSIM_SIGNAL_NUMBER = NULL;
            CLIPSIM_SIGNAL_PROGRAM = NULL;
        }
        signal_number += SIGRTMIN;
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
        XEvent xevent;
        char *save = NULL;
        ulong length;

        nanosleep(&pause, NULL);
        (void) XNextEvent(display, &xevent);
        mtx_lock(&lock);

        if (CLIPSIM_SIGNAL_PROGRAM)
            send_signal(CLIPSIM_SIGNAL_PROGRAM, signal_number);

        switch (clipboard_get_clipboard(&save, &length)) {
        case CLIPBOARD_TEXT:
            history_append(save, (int) length);
            break;
        case CLIPBOARD_IMAGE:
            history_append(save, (int) length);
            break;
        case CLIPBOARD_OTHER:
            error("Unsupported format."
                  " Clipsim only works with UTF-8 and images.\n");
            break;
        case CLIPBOARD_LARGE:
            error("Buffer is too large and INCR reading is not implemented yet."
                  " This data won't be saved to history.\n");
            break;
        case CLIPBOARD_ERROR:
            history_recover(-1);
            break;
        }
        mtx_unlock(&lock);
    }
}

Atom
clipboard_check_target(const Atom target) {
#ifdef CLIPSIM_DEBUG
    if (target <= XA_LAST_PREDEFINED)
        DEBUG_PRINT("%s", XGetAtomName(display, target));
    else
        DEBUG_PRINT("%lu", target);
#endif
    XEvent xevent;
    int nevents = 0;

    XConvertSelection(display, CLIPBOARD, target, XSEL_DATA,
                      window, CurrentTime);
    do {
        if (nevents >= CHECK_TARGET_MAX_EVENTS)
            return 0;
        (void) XNextEvent(display, &xevent);
        nevents += 1;
    } while (xevent.type != SelectionNotify
          || xevent.xselection.selection != CLIPBOARD);

    return xevent.xselection.property;
}

int32
clipboard_get_clipboard(char **save, ulong *length) {
    DEBUG_PRINT("%p, %p", (void *) save, (void *) length);
    int actual_format_return;
    ulong nitems_return;
    ulong bytes_after_return;
    Atom actual_type_return;

    if (clipboard_check_target(UTF8_STRING)) {
        XGetWindowProperty(display, window, XSEL_DATA, 0, LONG_MAX/4,
                           False, AnyPropertyType, &actual_type_return,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **) save);
        if (actual_type_return == INCR)
            return CLIPBOARD_LARGE;

        *length = nitems_return;
        return CLIPBOARD_TEXT;
    }
    if (clipboard_check_target(image_png)) {
        XGetWindowProperty(display, window, XSEL_DATA, 0, LONG_MAX/4,
                           False, AnyPropertyType, &actual_type_return,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **) save);
        if (actual_type_return == INCR)
            return CLIPBOARD_LARGE;

        *length = nitems_return;
        return CLIPBOARD_IMAGE;
    }
    if (clipboard_check_target(TARGETS))
        return CLIPBOARD_OTHER;

    return CLIPBOARD_ERROR;
}
