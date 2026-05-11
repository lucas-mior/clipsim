/*
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

#if !defined(CLIPBOARD_C)
#define CLIPBOARD_C

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include "clipsim.h"
#include "cbase/util.c"
#include "history.c"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_clipboard 1
#elif !defined(TESTING_clipboard)
#define TESTING_clipboard 0
#endif

#define CHECK_TARGET_MAX_EVENTS 10

static char *event_names[LASTEvent] = {
    "ProtocolError",  "ProtocolReply",  "KeyPress",         "KeyRelease",
    "ButtonPress",    "ButtonRelease",  "MotionNotify",     "EnterNotify",
    "LeaveNotify",    "FocusIn",        "FocusOut",         "KeymapNotify",
    "Expose",         "GraphicsExpose", "NoExpose",         "VisibilityNotify",
    "CreateNotify",   "DestroyNotify",  "UnmapNotify",      "MapNotify",
    "MapRequest",     "ReparentNotify", "ConfigureNotify",  "ConfigureRequest",
    "GravityNotify",  "ResizeRequest",  "CirculateNotify",  "CirculateRequest",
    "PropertyNotify", "SelectionClear", "SelectionRequest", "SelectionNotify",
    "ColormapNotify", "ClientMessage",  "MappingNotify",    "GenericEvent",
};

static Display *display;
static Window window;
static Window root;
static Atom CLIPBOARD;
static Atom XSEL_DATA;
static Atom INCR;
static Atom UTF8_STRING;
static Atom image_png;
static Atom TARGETS;

static void clipboard_incremental_case(char **, ulong *);
static Atom clipboard_check_target(Atom);
static int32 clipboard_get_clipboard(char **, ulong *, bool *);

static int clipboard_daemon_watch(void) __attribute__((noreturn));

int32
clipboard_daemon_watch(void) {
    DEBUG_PRINT("void")
    ulong color;
    struct timespec pause;
    char *CLIPSIM_SIGNAL_NUMBER;
    char *CLIPSIM_SIGNAL_PROGRAM;
    int32 signal_number = 0;
    int32 xfixes_event_base;
    int32 xfixes_error_base;

    pause.tv_sec = 0;
    pause.tv_nsec = 1000*1000*10;

    if ((display = XOpenDisplay(NULL)) == NULL) {
        error("Error opening X display.");
        exit(EXIT_FAILURE);
    }

    if (!XFixesQueryExtension(display, &xfixes_event_base, &xfixes_error_base)) {
        error("XFixes extension not available.\n");
        exit(EXIT_FAILURE);
    }

    if ((CLIPSIM_SIGNAL_PROGRAM = getenv("CLIPSIM_SIGNAL_PROGRAM")) == NULL) {
        error("CLIPSIM_SIGNAL_PROGRAM is not defined.\n");
    }
    if ((CLIPSIM_SIGNAL_NUMBER = getenv("CLIPSIM_SIGNAL_NUMBER")) == NULL) {
        error("CLIPSIM_SIGNAL_NUMBER is not defined.\n");
    }
    if (CLIPSIM_SIGNAL_PROGRAM && CLIPSIM_SIGNAL_NUMBER) {
        if ((signal_number = atoi(CLIPSIM_SIGNAL_NUMBER)) <= 0) {
            error("Invalid CLIPSIM_SIGNAL_NUMBER environment variable: %s.\n",
                  CLIPSIM_SIGNAL_NUMBER);
            if (CLIPSIM_SIGNAL_PROGRAM) {
                error("%s will not be signaled.\n", CLIPSIM_SIGNAL_PROGRAM);
            }

            CLIPSIM_SIGNAL_NUMBER = NULL;
            CLIPSIM_SIGNAL_PROGRAM = NULL;
        }
        signal_number += SIGRTMIN;
    }

    CLIPBOARD = XInternAtom(display, "CLIPBOARD", False);
    XSEL_DATA = XInternAtom(display, "XSEL_DATA", False);
    INCR = XInternAtom(display, "INCR", False);
    UTF8_STRING = XInternAtom(display, "UTF8_STRING", False);
    image_png = XInternAtom(display, "image/png", False);
    TARGETS = XInternAtom(display, "TARGETS", False);

    root = DefaultRootWindow(display);
    color = BlackPixel(display, DefaultScreen(display));
    window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, color, color);

    XFixesSelectSelectionInput(display, root, CLIPBOARD,
                               (ulong)XFixesSetSelectionOwnerNotifyMask
                                   | XFixesSelectionClientCloseNotifyMask
                                   | XFixesSelectionWindowDestroyNotifyMask);
    XFlush(display);

    while (true) {
        XEvent xevent;
        char *save = NULL;
        ulong length;
        bool incr;

        (void)XNextEvent(display, &xevent);
        if (DEBUGGING) {
            if (xevent.type < LENGTH(event_names)) {
                error("X event: %s\n", event_names[xevent.type]);
            } else {
                error("X event: %d\n", xevent.type);
            }
        }

        if (xevent.type != (xfixes_event_base + XFixesSelectionNotify)) {
            continue;
        }
        nanosleep(&pause, NULL);

        if (CLIPSIM_SIGNAL_PROGRAM) {
            send_signal(CLIPSIM_SIGNAL_PROGRAM, signal_number);
        }

        xpthread_mutex_lock(&lock);

        switch (clipboard_get_clipboard(&save, &length, &incr)) {
        case CLIPBOARD_TEXT:
            history_append(save, (int32)length, incr);
            break;
        case CLIPBOARD_IMAGE:
            history_append(save, (int32)length, incr);
            break;
        case CLIPBOARD_OTHER:
            error("Unsupported format."
                  " Clipsim only works with UTF-8 and images.\n");
            break;
        case CLIPBOARD_LARGE:
            error("Buffer is too large."
                  " This data won't be saved to history.\n");
            break;
        case CLIPBOARD_ERROR:
            error("Empty clipboard detected. Recovering last entry...\n");
            history_recover(-1);
            break;
        default:
            error("Unhandled result from clipboard_get_clipboard.\n");
            exit(EXIT_FAILURE);
        }
        xpthread_mutex_unlock(&lock);
    }
}

int32
clipboard_get_clipboard(char **save, ulong *length, bool *incr) {
    DEBUG_PRINT("%p, %p", (void *)save, (void *)length)
    int32 actual_format_return;
    ulong nitems_return;
    ulong bytes_after_return;
    Atom actual_type_return;

    *incr = false;

    if (clipboard_check_target(UTF8_STRING)) {
        XGetWindowProperty(display, window, XSEL_DATA, 0, LONG_MAX / 4, False,
                           AnyPropertyType, &actual_type_return,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **)save);
        if (actual_type_return == INCR) {
            XFree(*save);
            clipboard_incremental_case(save, length);
            if ((*length <= 0) || (*length >= ENTRY_MAX_LENGTH)) {
                return CLIPBOARD_LARGE;
            }
            *incr = true;
            return CLIPBOARD_TEXT;
        }

        *length = nitems_return;
        return CLIPBOARD_TEXT;
    }
    if (clipboard_check_target(image_png)) {
        XGetWindowProperty(display, window, XSEL_DATA, 0, LONG_MAX / 4, False,
                           AnyPropertyType, &actual_type_return,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **)save);
        if (actual_type_return == INCR) {
            XFree(*save);
            clipboard_incremental_case(save, length);
            if ((*length <= 0) || (*length >= ENTRY_MAX_LENGTH)) {
                return CLIPBOARD_LARGE;
            }
            *incr = true;
            return CLIPBOARD_IMAGE;
        }

        *length = nitems_return;
        return CLIPBOARD_IMAGE;
    }
    if (clipboard_check_target(TARGETS)) {
        return CLIPBOARD_OTHER;
    }

    return CLIPBOARD_ERROR;
}

Atom
clipboard_check_target(Atom target) {
    int32 retries = 50;
    struct timespec pause;
    DEBUG_PRINT("%lu", target)

    XEvent xevent;

    XConvertSelection(display, CLIPBOARD, target, XSEL_DATA, window, CurrentTime);
    
    pause.tv_sec = 0;
    pause.tv_nsec = 1000 * 1000 * 10;

    while (true) {
        if (XCheckTypedWindowEvent(display, window, SelectionNotify, &xevent)) {
            if (xevent.xselection.selection == CLIPBOARD) {
                return xevent.xselection.property;
            }
        }
        retries -= 1;
        if (retries <= 0) {
            return 0;
        }
        nanosleep(&pause, NULL);
    }
}

void
clipboard_incremental_case(char **save, ulong *length) {
    DEBUG_PRINT("%p, %p", (void *)save, (void *)length)
    int32 actual_format_return;
    ulong nitems_return;
    ulong bytes_after_return;
    Atom actual_type_return;
    char *buffer;
    bool exceeded = false;
    ulong current_size = 0;
    char *final_buffer;

    final_buffer = malloc2(ENTRY_MAX_LENGTH);

    XSelectInput(display, window, PropertyChangeMask);
    XDeleteProperty(display, window, XSEL_DATA);
    XFlush(display);

    while (true) {
        XEvent event;
        bool got_event = false;
        int32 retries = 100;
        struct timespec pause;
        
        pause.tv_sec = 0;
        pause.tv_nsec = 1000 * 1000 * 10;

        while (true) {
            if (XCheckTypedWindowEvent(display, window, PropertyNotify, &event)) {
                if ((event.xproperty.state == PropertyNewValue) && (event.xproperty.atom == XSEL_DATA)) {
                    got_event = true;
                    break;
                }
            }
            retries -= 1;
            if (retries <= 0) {
                break;
            }
            nanosleep(&pause, NULL);
        }

        if (!got_event) {
            exceeded = true;
            break;
        }

        XGetWindowProperty(display, window, XSEL_DATA, 0, LONG_MAX / 4, False,
                           AnyPropertyType, &actual_type_return,
                           &actual_format_return, &nitems_return,
                           &bytes_after_return, (uchar **)&buffer);

        if (nitems_return == 0) {
            XFree(buffer);
            XDeleteProperty(display, window, XSEL_DATA);
            break;
        }

        if (!exceeded) {
            if ((current_size + nitems_return) >= ENTRY_MAX_LENGTH) {
                exceeded = true;
            } else {
                memcpy64(final_buffer + current_size, buffer,
                         (int64)nitems_return);
                current_size += nitems_return;
            }
        }

        XFree(buffer);
        XDeleteProperty(display, window, XSEL_DATA);
        XFlush(display);
    }
    XSelectInput(display, window, NoEventMask);
    XFlush(display);

    if (exceeded) {
        free2(final_buffer, ENTRY_MAX_LENGTH);
        *save = NULL;
        *length = 0;
    } else {
        final_buffer[current_size] = '\0';
        *save = final_buffer;
        *length = current_size;
    }

    return;
}

#if TESTING_clipboard

int
main(void) {
    {
        int32 num_events = LENGTH(event_names);
        ASSERT_EQUAL(num_events, LASTEvent);
    }

    display = XOpenDisplay(NULL);
    if (display != NULL) {
        {
            pid_t pid = fork();
            if (pid == 0) {
                int32 nullfd = open("/dev/null", O_WRONLY);
                dup2(nullfd, STDERR_FILENO);
                close(nullfd);

                setenv("CLIPSIM_SIGNAL_PROGRAM", "clipsim_test", 1);
                setenv("CLIPSIM_SIGNAL_NUMBER", "1", 1);

                clipboard_daemon_watch();
            } else {
                int32 status = 0;
                usleep(200000);
                kill(pid, SIGTERM);
                wait(&status);
            }
        }

        {
            ulong color;
            char *test_str = "test_clip";
            ulong len = 0;
            char *res_save = NULL;
            int32 res_clip;
            bool incr;

            CLIPBOARD = XInternAtom(display, "CLIPBOARD", False);
            XSEL_DATA = XInternAtom(display, "XSEL_DATA", False);
            INCR = XInternAtom(display, "INCR", False);
            UTF8_STRING = XInternAtom(display, "UTF8_STRING", False);
            image_png = XInternAtom(display, "image/png", False);
            TARGETS = XInternAtom(display, "TARGETS", False);

            root = DefaultRootWindow(display);
            color = BlackPixel(display, DefaultScreen(display));
            window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, color, color);

            XChangeProperty(display, window, XSEL_DATA, UTF8_STRING, 8,
                            PropModeReplace, (uchar *)test_str, 9);

            {
                XEvent mock_event;
                Atom tgt;

                mock_event.type = SelectionNotify;
                mock_event.xselection.requestor = window;
                mock_event.xselection.selection = CLIPBOARD;
                mock_event.xselection.target = UTF8_STRING;
                mock_event.xselection.property = UTF8_STRING;
                XPutBackEvent(display, &mock_event);

                tgt = clipboard_check_target(UTF8_STRING);
                ASSERT_EQUAL(tgt, UTF8_STRING);
            }

            {
                XEvent mock_event2;

                mock_event2.type = SelectionNotify;
                mock_event2.xselection.requestor = window;
                mock_event2.xselection.selection = CLIPBOARD;
                mock_event2.xselection.target = UTF8_STRING;
                mock_event2.xselection.property = UTF8_STRING;
                XPutBackEvent(display, &mock_event2);

                res_clip = clipboard_get_clipboard(&res_save, &len, &incr);
                ASSERT_EQUAL(res_clip, CLIPBOARD_TEXT);
                ASSERT_MORE(len, 0);

                if (res_save != NULL) {
                    XFree(res_save);
                }
            }

            {
                char *large_save = NULL;
                ulong large_len = 0;
                pid_t pid = fork();

                if (pid == 0) {
                    Display *d2;
                    XEvent event;
                    char *big_chunk;
                    
                    d2 = XOpenDisplay(NULL);
                    big_chunk = malloc(ENTRY_MAX_LENGTH + 10);
                    memset64(big_chunk, 'B', ENTRY_MAX_LENGTH + 10);
                    
                    XSelectInput(d2, window, PropertyChangeMask);
                    XChangeProperty(d2, window, XSEL_DATA, INCR, 32, PropModeReplace, NULL, 0);
                    XFlush(d2);

                    do {
                        XNextEvent(d2, &event);
                    } while (event.type != PropertyNotify || event.xproperty.state != PropertyDelete || event.xproperty.atom != XSEL_DATA);
                    
                    XChangeProperty(d2, window, XSEL_DATA, UTF8_STRING, 8,
                                    PropModeReplace, (uchar *)big_chunk, ENTRY_MAX_LENGTH + 10);
                    XFlush(d2);
                    
                    do {
                        XNextEvent(d2, &event);
                    } while (event.type != PropertyNotify || event.xproperty.state != PropertyDelete || event.xproperty.atom != XSEL_DATA);
                    
                    XChangeProperty(d2, window, XSEL_DATA, UTF8_STRING, 8,
                                    PropModeReplace, NULL, 0);
                    XFlush(d2);
                    XCloseDisplay(d2);
                    free(big_chunk);
                    exit(0);
                } else {
                    usleep(100000);
                    clipboard_incremental_case(&large_save, &large_len);
                    ASSERT_EQUAL(large_len, 0);
                    if (large_save != NULL) {
                        free2(large_save, ENTRY_MAX_LENGTH);
                    }
                    wait(NULL);
                }
            }

            {
                char *small_save = NULL;
                ulong small_len = 0;
                pid_t pid = fork();

                if (pid == 0) {
                    Display *d2;
                    XEvent event;
                    char *small_chunk = "small_incr_test";
                    
                    d2 = XOpenDisplay(NULL);
                    XSelectInput(d2, window, PropertyChangeMask);
                    XChangeProperty(d2, window, XSEL_DATA, INCR, 32, PropModeReplace, NULL, 0);
                    XFlush(d2);

                    do {
                        XNextEvent(d2, &event);
                    } while (event.type != PropertyNotify || event.xproperty.state != PropertyDelete || event.xproperty.atom != XSEL_DATA);
                    
                    XChangeProperty(d2, window, XSEL_DATA, UTF8_STRING, 8,
                                    PropModeReplace, (uchar *)small_chunk, 15);
                    XFlush(d2);
                                    
                    do {
                        XNextEvent(d2, &event);
                    } while (event.type != PropertyNotify || event.xproperty.state != PropertyDelete || event.xproperty.atom != XSEL_DATA);
                    
                    XChangeProperty(d2, window, XSEL_DATA, UTF8_STRING, 8,
                                    PropModeReplace, NULL, 0);
                    XFlush(d2);
                    XCloseDisplay(d2);
                    exit(0);
                } else {
                    usleep(100000);
                    clipboard_incremental_case(&small_save, &small_len);
                    ASSERT_EQUAL(small_len, 15);
                    ASSERT_EQUAL(memcmp64(small_save, "small_incr_test", 15), 0);
                    if (small_save != NULL) {
                        free2(small_save, ENTRY_MAX_LENGTH);
                    }
                    wait(NULL);
                }
            }
        }
        XCloseDisplay(display);
    }

    exit(EXIT_SUCCESS);
}
#endif

#endif /* CLIPBOARD_C */
