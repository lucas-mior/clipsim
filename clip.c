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
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include "clip.h"
#include "clipsim.h"
#include "config.h"
#include "hist.h"
#include "util.h"
#include "send_signal.h"

static Display *display;
static Window root;
static Atom clip_atom;
static XEvent xev;
static Window window;

static int32 get_clipboard(char **, ulong *);
static bool valid_content(uchar *);
static void signal_program(void);

typedef union Signal {
    char *str;
    int num;
} Signal;

void signal_program(void) {
    Signal sig;
    char *program;
    if (!(sig.str = getenv("CLIPSIM_SIGNAL_CODE"))) {
        fprintf(stderr, "CLIPSIM_SIGNAL_CODE environment variable not set.\n");
        return;
    }
    if (!(program = getenv("CLIPSIM_SIGNAL_PROGRAM"))) {
        fprintf(stderr, "CLIPSIM_SIGNAL_PROGRAM environment variable not set.\n");
        return;
    }
    if ((sig.num = atoi(sig.str)) < 10) {
        fprintf(stderr, "Invalid CLIPSIM_SIGNAL_CODE environment variable.\n");
        return;
    }

    send_signal(program, sig.num);
}

void *daemon_watch_clip(void *unused) {
    ulong color;
    struct timespec pause;
    pause.tv_sec = 0;
    pause.tv_nsec = PAUSE10MS;
    (void) unused;

    if (!(display = XOpenDisplay(NULL))) {
        fprintf(stderr, "Can't open X display.\n");
        exit(1);
    }

    root = DefaultRootWindow(display);
    clip_atom = XInternAtom(display, "CLIPBOARD", False);

    color = BlackPixel(display, DefaultScreen(display));
    window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                 0,0, 1,1, 0, color, color);

    XFixesSelectSelectionInput(display, root, clip_atom, (ulong)
                               XFixesSetSelectionOwnerNotifyMask
                             | XFixesSelectionClientCloseNotifyMask
                             | XFixesSelectionWindowDestroyNotifyMask);

    while (true) {
        char *save = NULL;
        ulong len;
        bool large_entry = false;
        nanosleep(&pause , NULL);
        (void) XNextEvent(display, &xev);
        pthread_mutex_lock(&lock);

        signal_program();

        switch (get_clipboard(&save, &len)) {
            case -2:
                fprintf(stderr, "Image copied to clipboard. "
                                "This won't be added to history.\n");
                goto unlock;
                break;
            case -1:
                save = NULL;
                break;
            case 0:
                large_entry = true;
                break;
        }

        if (save == NULL) {
            hist_rec(0);
        } else if (!large_entry) {
            if (valid_content((uchar *) save))
                hist_add(save, len);
        }
        large_entry = false;

        unlock:
        pthread_mutex_unlock(&lock);
    }
}

static int32 get_clipboard(char **save, ulong *len) {
    int resbits = 0;
    ulong ressize = 0, restail = 0;
    Atom utf8_atom = XInternAtom(display, "UTF8_STRING", False);
    Atom image_atom = XInternAtom(display, "image/png", False);
    Atom prop_atom = XInternAtom(display, "XSEL_DATA", False);
    Atom incr_atom = XInternAtom(display, "INCR", False);
    XEvent event;

    XConvertSelection(display, clip_atom, utf8_atom, prop_atom,
                      window, CurrentTime);
    do {
        (void) XNextEvent(display, &event);
    } while (event.type != SelectionNotify
          || event.xselection.selection != clip_atom);

    if (event.xselection.property) {
        XGetWindowProperty(display, window, prop_atom, 0, LONG_MAX/4,
                           False, AnyPropertyType, &utf8_atom,
                           &resbits, &ressize, &restail, (uchar **) save);
        if (utf8_atom == incr_atom) {
            fprintf(stderr, "Buffer is too large and "
                            "INCR reading is not implemented yet. "
                            "This entry won't be saved to history.\n");
            return 0;
        } else {
            *len = ressize;
            return 1;
        }
    } else { // request failed, e.g. owner can't convert to the target format
        XConvertSelection(display, clip_atom, image_atom, prop_atom,
                          window, CurrentTime);
        do {
            (void) XNextEvent(display, &event);
        } while (event.type != SelectionNotify
              || event.xselection.selection != clip_atom);

        if (event.xselection.property) {
            return -2;
        } else {
            return -1;
        }
    }
}

static bool valid_content(uchar *data) {
    static const uchar PNG[] = {0x89, 0x50, 0x4e, 0x47, 0x00};

    { /* Check if it is made only of spaces and newlines */
        uchar *aux = data;
        do {
            aux++;
        } while ((*(aux-1) == ' ')
              || (*(aux-1) == '\t')
              || (*(aux-1) == '\n'));
        if (*(aux-1) == '\0') {
            fprintf(stderr, "Only white space copied to clipboard. "
                            "This won't be added to history.\n");
            return false;
        }
    }

    { /* check if it is an image */
        int i = 0;
        do {
            if (data[i] == '\0')
                return true;
            if (data[i] != PNG[i])
                return true;
        } while (++i <= 3);

        fprintf(stderr, "Image copied to clipboard. "
                        "This won't be added to history.\n");
        return false;
    }
}
