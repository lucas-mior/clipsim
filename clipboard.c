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

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>

#include "clipsim.h"

static Display *DISPLAY;
static Atom CLIPBOARD, PROPERTY, INCREMENT;
static Atom UTF8, IMG, TARGET;
static XEvent XEV;
static Window WINDOW;

static Atom clipboard_check_target(Atom);
static GetClipboardResult clipboard_get_clipboard(char **, ulong *);
static void clipboard_signal_program(void);

void *clipboard_daemon_watch(void *unused) {
    DEBUG_PRINT("*clipboard_daemon_watch(void *unused) %d\n", __LINE__)
    ulong color;
    Window root;
    struct timespec pause;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;
    (void) unused;

    if (!(DISPLAY = XOpenDisplay(NULL))) {
        fprintf(stderr, "Error opening X display.");
        exit(EXIT_FAILURE);
    }

    CLIPBOARD = XInternAtom(DISPLAY, "CLIPBOARD",   False);
    PROPERTY  = XInternAtom(DISPLAY, "XSEL_DATA",   False);
    INCREMENT = XInternAtom(DISPLAY, "INCR",        False);
    UTF8      = XInternAtom(DISPLAY, "UTF8_STRING", False);
    IMG       = XInternAtom(DISPLAY, "image/png",   False);
    TARGET    = XInternAtom(DISPLAY, "TARGETS",     False);

    root = DefaultRootWindow(DISPLAY);
    color = BlackPixel(DISPLAY, DefaultScreen(DISPLAY));
    WINDOW = XCreateSimpleWindow(DISPLAY, root, 0,0, 1,1, 0, color, color);

    XFixesSelectSelectionInput(DISPLAY, root, CLIPBOARD, (ulong)
                               XFixesSetSelectionOwnerNotifyMask
                             | XFixesSelectionClientCloseNotifyMask
                             | XFixesSelectionWindowDestroyNotifyMask);

    while (true) {
        char *save = NULL;
        ulong length;
        nanosleep(&pause, NULL);
        (void) XNextEvent(DISPLAY, &XEV);
        pthread_mutex_lock(&lock);

        clipboard_signal_program();

        switch (clipboard_get_clipboard(&save, &length)) {
            case TEXT:
                history_append(save, length);
                break;
            case IMAGE:
                history_append(save, length);
                break;
            case OTHER:
                fprintf(stderr, "Unsupported format. Clipsim only"
                                " works with UTF-8 and images.\n");
                break;
            case LARGE:
                fprintf(stderr, "Buffer is too large and "
                                "INCR reading is not implemented yet. "
                                "This data won't be saved to history.\n");
                break;
            case ERROR:
                history_recover(-1);
                break;
        }
        pthread_mutex_unlock(&lock);
    }
}

Atom clipboard_check_target(Atom target) {
    DEBUG_PRINT("clipboard_check_target(%lu)\n", target)
    XEvent xevent;

    XConvertSelection(DISPLAY, CLIPBOARD, target, PROPERTY,
                      WINDOW, CurrentTime);
    do {
        (void) XNextEvent(DISPLAY, &xevent);
    } while (xevent.type != SelectionNotify
          || xevent.xselection.selection != CLIPBOARD);

    return xevent.xselection.property;
}

GetClipboardResult clipboard_get_clipboard(char **save, ulong *length) {
    DEBUG_PRINT("clipboard_get_clipboard(%p, %lu)\n", (void *) save, *length)
    int actual_format_return;
    ulong nitems_return;
    ulong bytes_after_return;
    Atom return_atom;

    if (clipboard_check_target(UTF8)) {
        XGetWindowProperty(DISPLAY, WINDOW, PROPERTY, 0, LONG_MAX/4,
                           False, AnyPropertyType, &return_atom,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **) save);
        if (return_atom == INCREMENT) {
            return LARGE;
        } else {
            *length = nitems_return;
            return TEXT;
        }
    } else if (clipboard_check_target(IMG)) {
        XGetWindowProperty(DISPLAY, WINDOW, PROPERTY, 0, LONG_MAX/4,
                           False, AnyPropertyType, &return_atom,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **) save);
        if (return_atom == INCREMENT) {
            return LARGE;
        } else {
            *length = nitems_return;
            return IMAGE;
        }
    } else if (clipboard_check_target(TARGET)) {
        return OTHER;
    }
    return ERROR;
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
