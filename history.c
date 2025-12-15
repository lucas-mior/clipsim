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

#if !defined(HISTORY_C)
#define HISTORY_C

#include "clipsim.h"
#include "util.c"
#include "content.c"
#include "arena.c"

#include <poll.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <ftw.h>

#define MAX_OPEN_FD 64

static volatile bool recovered = false;
static int32 history_length;
static File history = {.file = NULL, .fd = -1, .name = NULL};
static char *XDG_CACHE_HOME = NULL;
static char xdg_cache_home_buffer[256];
static char *HOME = NULL;
static uint8 length_counts[ENTRY_MAX_LENGTH] = {0};
static char *tmp_directory = "/tmp/clipsim";
static Arena *arena;

static int32 history_repeated_index(char *, int32);
static void history_free_entry(Entry *, int32);
static void history_reorder(int32);
static int32 history_save_image(char **, int32 *);

static void history_append(char *, int);
static pthread_t *history_save(void);
static void history_recover(int32);
static void history_remove(int32);
static void history_exit(int) __attribute__((noreturn));

static int32
history_callback_delete(const char *path, const struct stat *stat,
                        int32 typeflag, struct FTW *ftwbuf) {
    (void)stat;
    (void)ftwbuf;

    if (typeflag == FTW_F) {
        if (unlink(path) < 0) {
            error("Error deleting %s: %s.\n", path, strerror(errno));
        } else {
            printf("Deleted '%s'.", path);
        }
    } else if (typeflag == FTW_DP) {
        if (rmdir(path) < 0) {
            error("Error deleting %s: %s.\n", path, strerror(errno));
        } else {
            printf("Deleted '%s'.", path);
        }
    }

    return 0;
}

pthread_t *
history_save(void) {
    DEBUG_PRINT("void")
    static int32 nfds = 0;
    static struct pollfd pipes[HISTORY_BUFFER_SIZE] = {0};
    static int dests[HISTORY_BUFFER_SIZE];
    static pthread_t thread;
    static UtilCopyFilesAsync pipe_thread;

    for (int32 i = 0; i < LENGTH(pipes); i += 1) {
        pipes[i].fd = -1;
        pipes[i].events = POLLIN;
    }

    error("Saving history...\n");
    if (history_length <= 0) {
        error("History is empty. Not saving.\n");
        return NULL;
    }
    if (history.name == NULL) {
        error("History file name unresolved, can't save history.");
        return NULL;
    }
    if ((history.fd
         = open(history.name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR))
        < 0) {
        error("Error opening history file for saving: %s\n", strerror(errno));
        return NULL;
    }

    for (int32 i = 0; i < history_length; i += 1) {
        int64 tag_size = sizeof(*(&IMAGE_TAG));
        Entry *e = &entries[i];

        if (is_image[i]) {
            char image_save[PATH_MAX];
            int32 n;

            n = SNPRINTF(image_save, "%s/clipsim/%s", XDG_CACHE_HOME,
                         basename(e->content));

            if (strcmp(image_save, e->content)) {
                if ((pipes[nfds].fd = util_copy_file_async(
                         image_save, e->content, &(dests[nfds])))
                    < 0) {
                    error("Error copying %s to %s: %s.\n", e->content,
                          image_save, strerror(errno));
                    history_remove(i);
                    continue;
                }
                nfds += 1;
            }
            if (write64(history.fd, image_save, n) < n) {
                error("Error writing %s: %s\n", image_save, strerror(errno));
                history_remove(i);
                continue;
            }
            if (write64(history.fd, &TEXT_TAG, tag_size) < tag_size) {
                error("Error writing TEXT_TAG: %s\n", strerror(errno));
                history_remove(i);
                continue;
            }
            if (write64(history.fd, &IMAGE_TAG, tag_size) < tag_size) {
                error("Error writing IMAGE_TAG: %s\n", strerror(errno));
                history_remove(i);
                continue;
            }
        } else {
            int32 left = e->content_length;
            int32 offset = 0;
            int64 w;

            do {
                if ((w = write64(history.fd, e->content + offset, left)) <= 0) {
                    break;
                }
                left -= w;
                offset += w;
            } while (left > 0);
            if (w < 0) {
                error("Error writing %s: %s\n", e->content, strerror(errno));
                history_remove(i);
                continue;
            }
            if (write64(history.fd, &TEXT_TAG, tag_size) < tag_size) {
                error("Error writing TEXT_TAG: %s\n", strerror(errno));
                history_remove(i);
                continue;
            }
            if (write64(history.fd, &TEXT_TAG, tag_size) < tag_size) {
                error("Error writing TEXT_TAG: %s\n", strerror(errno));
                history_remove(i);
                continue;
            }
        }
    }

    pipe_thread.nfds = nfds;
    pipe_thread.pipes = pipes;
    pipe_thread.dests = dests;
    xpthread_create(&thread, NULL, util_copy_file_async_thread, &pipe_thread);

    util_close(&history);
    return &thread;
}

// clang-format off
#define XSIGNAL(NAME) [NAME] = #NAME
static char *signal_names[] = {
    XSIGNAL(SIGABRT),
    XSIGNAL(SIGALRM),
    XSIGNAL(SIGVTALRM),
    XSIGNAL(SIGPROF),
    XSIGNAL(SIGBUS),
    XSIGNAL(SIGCHLD),
    XSIGNAL(SIGCONT),
    XSIGNAL(SIGFPE),
    XSIGNAL(SIGHUP),
    XSIGNAL(SIGILL),
    XSIGNAL(SIGINT),
    XSIGNAL(SIGKILL),
    XSIGNAL(SIGPIPE),
    XSIGNAL(SIGPOLL),
    XSIGNAL(SIGQUIT),
    XSIGNAL(SIGSEGV),
    XSIGNAL(SIGSTOP),
    XSIGNAL(SIGSYS),
    XSIGNAL(SIGTERM),
    XSIGNAL(SIGTSTP),
    XSIGNAL(SIGTTIN),
    XSIGNAL(SIGTTOU),
    XSIGNAL(SIGTRAP),
    XSIGNAL(SIGURG),
    XSIGNAL(SIGUSR1),
    XSIGNAL(SIGUSR2),
    XSIGNAL(SIGXCPU),
    XSIGNAL(SIGXFSZ),
};
#undef XSIGNAL
// clang-format on

void
history_exit(int32 signum) {
    if (signum < LENGTH(signal_names)) {
        error("Received signal %s.\n", signal_names[signum]);
    } else {
        error("Received signal %d.\n", signum);
    }
    pthread_t *thread_copying_images = history_save();

    error("Deleting images...\n");
    nftw(tmp_directory, history_callback_delete, MAX_OPEN_FD,
         FTW_DEPTH | FTW_PHYS);

    if (thread_copying_images) {
        xpthread_join(*thread_copying_images, NULL);
    }
    _exit(EXIT_SUCCESS);
}

static void
history_read(void) {
    DEBUG_PRINT("void")
    int64 history_size;
    char *history_map;
    char *begin;
    char *p;
    int32 left;

    char *clipsim = "clipsim/history";
    int64 length;

    if ((XDG_CACHE_HOME = getenv("XDG_CACHE_HOME")) == NULL) {
        error("XDG_CACHE_HOME is not set, using HOME...\n");
        if ((HOME = getenv("HOME")) == NULL) {
            error("HOME is not set.\n");
            exit(EXIT_FAILURE);
        }
        SNPRINTF(xdg_cache_home_buffer, "%s/%s", HOME, ".cache");
        XDG_CACHE_HOME = xdg_cache_home_buffer;
    }

    length = strlen64(XDG_CACHE_HOME);
    length += 1 + strlen64(clipsim);
    if (length > (PATH_MAX - 1)) {
        error("XDG_CACHE_HOME is too long.\n");
        exit(EXIT_FAILURE);
    }

    arena = arena_create(SIZEMB(2));

    {
        char *clipsim_dir;
        char buffer[PATH_MAX];
        int n = SNPRINTF(buffer, "%s/%s", XDG_CACHE_HOME, clipsim);
        history.name = util_memdup(buffer, n + 1);

        clipsim_dir = dirname(buffer);
        if (mkdir(clipsim_dir, 0770) < 0) {
            if (errno != EEXIST) {
                error("Error creating dir '%s': %s\n", clipsim_dir,
                      strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }

    if ((history.fd = open(history.name, O_RDONLY)) < 0) {
        error("Error opening history file for reading: %s\n"
              "History will start empty.\n",
              strerror(errno));
        return;
    }

    {
        struct stat history_stat;
        if (fstat(history.fd, &history_stat) < 0) {
            error("Error getting file information: %s\n"
                  "History will start empty.\n",
                  strerror(errno));
            util_close(&history);
            return;
        }
        history_size = history_stat.st_size;
        if (history_size <= 0) {
            error("history_size: %lld\n", (llong)history_size);
            error("History file is empty.\n");
            util_close(&history);
            return;
        }
        if (history_size >= MAXOF(left)) {
            error("History file is too big.\n");
            error("Max size is %d bytes.", MAXOF(left));
            return;
        }
    }

    history_map = mmap(NULL, (size_t)history_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE, history.fd, 0);

    if (history_map == MAP_FAILED) {
        error("Error mapping history file to memory: %s"
              "History will start empty.\n",
              strerror(errno));
        util_close(&history);
        return;
    }

    history_length = 0;
    begin = history_map;
    left = (int32)history_size;

    while ((left > 0) && (p = memchr64(begin, TEXT_TAG, left))) {
        Entry *e;
        char type = *(p + 1);
        *p = '\0';

        e = &entries[history_length];
        e->content_length = (int32)(p - begin);

        if (type == IMAGE_TAG) {
            e->trimmed = 0;
            e->trimmed_length = (int16)e->content_length;
            is_image[history_length] = true;
            e->content = arena_push(arena, (e->content_length + 1));
            memcpy64(e->content, begin, e->content_length + 1);
        } else {
            int32 size;
            if (e->content_length >= TRIMMED_SIZE) {
                size = e->content_length + 1 + TRIMMED_SIZE + 1;
            } else {
                size = (e->content_length + 1)*2;
            }
            e->content = arena_push(arena, size);
            memcpy64(e->content, begin, e->content_length + 1);

            content_trim_spaces(&e->trimmed, &e->trimmed_length, e->content,
                                e->content_length);
            is_image[history_length] = false;
        }
        p += 1;
        begin = p + 1;

        length_counts[e->content_length] += 1;
        history_length += 1;

        left -= (e->content_length + 1);

        if (history_length >= HISTORY_BUFFER_SIZE) {
            break;
        }
    }

    if (munmap(history_map, (size_t)history_size) < 0) {
        error("Error unmapping %p with %lld bytes: %s\n", (void *)history_map,
              (llong)history_size, strerror(errno));
    }
    util_close(&history);
    return;
}

int32
history_repeated_index(char *content, int32 length) {
    DEBUG_PRINT("%s, %d", content, length)
    int32 candidates = length_counts[length];
    if (candidates == 0) {
        return -1;
    }

    for (int32 i = history_length - 1; i >= 0; i -= 1) {
        Entry *e = &entries[i];

        if (e->content_length != length) {
            continue;
        }
        if (!memcmp64(e->content, content, length)) {
            return i;
        }

        candidates -= 1;
        if (candidates <= 0) {
            return -1;
        }
    }
    return -1;
}

int32
history_save_image(char **content, int32 *length) {
    DEBUG_PRINT("%p, %d", (void *)content, *length)
    int32 file;
    int64 w;
    int64 copied = 0;
    char image_file[256];
    int64 t = time(NULL);
    int32 n;

    n = SNPRINTF(image_file, "%s/%lld.png", tmp_directory, (llong)t);

    if ((file
         = open(image_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR))
        < 0) {
        error("Error opening %s for saving: %s\n", image_file, strerror(errno));
        return -1;
    }

    do {
        w = write64(file, *(content + copied), *length);
        if (w <= 0) {
            break;
        }
        copied += w;
        *length -= w;
    } while (*length > 0);

    if (w < 0) {
        error("Error writing to %s: %s\n", image_file, strerror(errno));
        return -1;
    }

    *length = n;
    memcpy64(*content, image_file, *length + 1);
    return 0;
}

void
history_append(char *content, int32 length) {
    DEBUG_PRINT("%s, %d", content, length)
    int32 oldindex;
    int32 kind;
    int32 size;
    Entry *e;

    if (!content) {
        error("Error getting data from clipboard. Skipping entry...\n");
        recovered = false;
        return;
    }
    if (recovered) {
        recovered = false;
        return;
    }

    kind = content_check_content((uchar *)content, length);
    switch (kind) {
    case CLIPBOARD_TEXT:
        content_remove_newline(content, &length);
        break;
    case CLIPBOARD_IMAGE:
        if (history_save_image(&content, &length) < 0) {
            return;
        }
        break;
    default:
        return;
    }

    if ((oldindex = history_repeated_index(content, length)) >= 0) {
        error("Entry is equal to previous entry. Reordering...\n");
        if (oldindex != (history_length - 1)) {
            history_reorder(oldindex);
        }
        XFree(content);
        return;
    }

    e = &entries[history_length];
    e->content_length = length;
    length_counts[length] += 1;

    switch (kind) {
    case CLIPBOARD_TEXT:
        if (e->content_length >= TRIMMED_SIZE) {
            size = e->content_length + 1 + TRIMMED_SIZE + 1;
        } else {
            size = (e->content_length + 1)*2;
        }
        e->content = arena_push(arena, size);
        memcpy64(e->content, content, e->content_length + 1);

        content_trim_spaces(&(e->trimmed), &(e->trimmed_length), e->content,
                            e->content_length);
        is_image[history_length] = false;
        break;
    case CLIPBOARD_IMAGE:
        e->trimmed = 0;
        e->trimmed_length = (int16)e->content_length;
        e->content = arena_push(arena, length + 1);
        memcpy64(e->content, content, length + 1);
        is_image[history_length] = true;
        break;
    default:
        error("Unexpected default case.\n");
        exit(EXIT_FAILURE);
    }
    XFree(content);

    history_length += 1;
    if (history_length >= HISTORY_BUFFER_SIZE) {
        for (int32 i = 0; i < HISTORY_KEEP_SIZE; i += 1) {
            history_free_entry(&entries[i], i);
        }

        memcpy64(&entries[0], &entries[HISTORY_KEEP_SIZE],
                 HISTORY_KEEP_SIZE*sizeof(*entries));
        memset64(&entries[HISTORY_KEEP_SIZE], 0,
                 HISTORY_KEEP_SIZE*sizeof(*entries));

        memcpy64(&is_image[0], &is_image[HISTORY_KEEP_SIZE],
                 HISTORY_KEEP_SIZE*sizeof(*is_image));
        memset64(&is_image[HISTORY_KEEP_SIZE], 0,
                 HISTORY_KEEP_SIZE*sizeof(*is_image));

        history_length = HISTORY_KEEP_SIZE;
    }

    return;
}

void
history_recover(int32 id) {
    DEBUG_PRINT("%d", id)
    int32 fd[2];
    Entry *e;
    bool istext;
    char *xclip = "xclip";
    char *xclip_path = "/usr/bin/xclip";

    if (history_length <= 0) {
        error("Clipboard history empty. Start copying text.\n");
        return;
    }
    if (id < 0) {
        id = history_length + id;
    }
    if ((id >= history_length) || (id < 0)) {
        error("Invalid index for recovery: %d\n", id);
        recovered = true;
        return;
    }

    e = &entries[id];

    if ((istext = (is_image[id] == false))) {
        if (pipe(fd) < 0) {
            util_die_notify("Error creating pipe: %s\n", strerror(errno));
        }
    }

    switch (fork()) {
    case 0:
        if (istext) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            execl(xclip_path, xclip, "-selection", "clipboard", NULL);
        } else {
            execl(xclip_path, xclip, "-selection", "clipboard", "-target",
                  "image/png", e->content, NULL);
        }
        util_die_notify("Error in exec(%s): %s", xclip_path, strerror(errno));
    case -1:
        util_die_notify("Error in fork(%s): %s", xclip_path, strerror(errno));
    default:
        if (istext && (close(fd[0]) < 0)) {
            error("Error closing pipe 0: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if (istext) {
        dprintf(fd[1], "%s", e->content);
        if (close(fd[1]) < 0) {
            error("Error closing pipe 1: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    if (wait(NULL) < 0) {
        error("Error waiting for fork: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (id != (history_length - 1)) {
        history_reorder(id);
    }

    recovered = true;
    return;
}

void
history_remove(int32 id) {
    DEBUG_PRINT("%d", id)
    if (history_length <= 0) {
        return;
    }

    if (id < 0) {
        id = history_length + id;
    } else if (id == history_length) {
        history_recover(-2);
        history_remove(-2);
        return;
    }
    if (id >= history_length) {
        error("Invalid index %d for deletion.\n", id);
        return;
    }

    history_free_entry(&entries[id], id);

    if (id < history_length) {
        memmove64(&entries[id], &entries[id + 1],
                  (history_length - id)*SIZEOF(*entries));
        memset64(&entries[history_length - 1], 0, sizeof(*entries));
        memmove64(&is_image[id], &is_image[id + 1],
                  (history_length - id)*SIZEOF(*is_image));
        memset64(&is_image[history_length - 1], 0, sizeof(*is_image));
    }
    history_length -= 1;

    return;
}

void
history_reorder(int32 oldindex) {
    DEBUG_PRINT("%d", oldindex)
    Entry aux = entries[oldindex];
    bool aux2 = is_image[oldindex];
    int32 n = history_length - oldindex;

    memmove64(&entries[oldindex], &entries[oldindex + 1], n*SIZEOF(*entries));
    memmove64(&entries[history_length - 1], &aux, SIZEOF(*entries));

    memmove64(&is_image[oldindex], &is_image[oldindex + 1],
              n*SIZEOF(*is_image));
    memmove64(&is_image[history_length - 1], &aux2, SIZEOF(*is_image));
    return;
}

void
history_free_entry(Entry *e, int32 index) {
    DEBUG_PRINT("{\n    %s,\n    %d\n}", e->content, e->content_length)
    length_counts[e->content_length] -= 1;

    if (is_image[index]) {
        if (unlink(e->content) < 0) {
            error("Error deleting %s: %s.\n", e->content, strerror(errno));
        }
    }
    assert(arena_pop(arena, e->content));

    return;
}

#endif /* HISTORY_C */
