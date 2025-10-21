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

static int32 history_callback_delete(const char *, const struct stat *, int32,
                                     struct FTW *);
static int32 history_repeated_index(const char *, const int32);
static void history_free_entry(const Entry *, int32);
static void history_reorder(const int32);
static int32 history_save_image(char **, int32 *);

static void history_read(void);
static void history_append(char *, int);
static bool history_save(void);
static void history_recover(int32);
static void history_remove(int32);
static void history_backup(void);
static void history_exit(int) __attribute__((noreturn));

int32
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

void
history_backup(void) {
    char buffer[PATH_MAX];
    SNPRINTF(buffer, "%s.bak", history.name);
    if (rename(history.name, buffer) < 0) {
        error("Error creating backup history file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return;
}

bool
history_save(void) {
    DEBUG_PRINT("void")

    error("Saving history...\n");
    if (history_length <= 0) {
        error("History is empty. Not saving.\n");
        return false;
    }
    if (history.name == NULL) {
        error("History file name unresolved, can't save history.");
        return false;
    }
    if ((history.fd
         = open(history.name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR))
        < 0) {
        error("Error opening history file for saving: %s\n", strerror(errno));
        return false;
    }

    for (int32 i = 0; i < history_length; i += 1) {
        usize tag_size = sizeof(*(&IMAGE_TAG));
        Entry *e = &entries[i];

        if (is_image[i]) {
            char image_save[PATH_MAX];
            int32 n;

            n = SNPRINTF(image_save, "%s/clipsim/%s", XDG_CACHE_HOME,
                         basename(e->content));

            if (strcmp(image_save, e->content)) {
                if (util_copy_file(image_save, e->content) < 0) {
                    error("Error copying %s to %s: %s.\n", e->content,
                          image_save, strerror(errno));
                    history_remove(i);
                    continue;
                }
            }
            if (write(history.fd, image_save, (usize)n) < n) {
                error("Error writing %s: %s\n", image_save, strerror(errno));
                history_remove(i);
                continue;
            }
            if (write(history.fd, &TEXT_TAG, tag_size) < (isize)tag_size) {
                error("Error writing TEXT_TAG: %s\n", strerror(errno));
                history_remove(i);
                continue;
            }
            if (write(history.fd, &IMAGE_TAG, tag_size) < (isize)tag_size) {
                error("Error writing IMAGE_TAG: %s\n", strerror(errno));
                history_remove(i);
                continue;
            }
        } else {
            int32 left = e->content_length;
            int32 offset = 0;
            isize w;

            do {
                w = write(history.fd, e->content + offset, (usize)left);
                if (w <= 0) {
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
            if (write(history.fd, &TEXT_TAG, tag_size) < (isize)tag_size) {
                error("Error writing TEXT_TAG: %s\n", strerror(errno));
                history_remove(i);
                continue;
            }
            if (write(history.fd, &TEXT_TAG, tag_size) < (isize)tag_size) {
                error("Error writing TEXT_TAG: %s\n", strerror(errno));
                history_remove(i);
                continue;
            }
        }
    }

    util_close(&history);
    return true;
}

void
history_exit(int32 unused) {
    (void)unused;

    history_save();

    error("Deleting images...\n");
    nftw(tmp_directory, history_callback_delete, MAX_OPEN_FD,
         FTW_DEPTH | FTW_PHYS);

    _exit(EXIT_SUCCESS);
}

void
history_read(void) {
    DEBUG_PRINT("void")
    usize history_size;
    char *history_map;
    char *begin;
    char *p;
    int32 left;

    const char *clipsim = "clipsim/history";
    usize length;

    if ((XDG_CACHE_HOME = getenv("XDG_CACHE_HOME")) == NULL) {
        error("XDG_CACHE_HOME is not set, using HOME...\n");
        if ((HOME = getenv("HOME")) == NULL) {
            error("HOME is not set.\n");
            exit(EXIT_FAILURE);
        }
        SNPRINTF(xdg_cache_home_buffer, "%s/%s", HOME, ".cache");
        XDG_CACHE_HOME = xdg_cache_home_buffer;
    }

    length = strlen(XDG_CACHE_HOME);
    length += 1 + strlen(clipsim);
    if (length > (PATH_MAX - 1)) {
        error("XDG_CACHE_HOME is too long.\n");
        exit(EXIT_FAILURE);
    }

    {
        char buffer[PATH_MAX];
        int n = SNPRINTF(buffer, "%s/%s", XDG_CACHE_HOME, clipsim);
        history.name = util_memdup(buffer, (usize)n + 1);

        char *clipsim_dir = dirname(buffer);
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
        history_size = (usize)history_stat.st_size;
        if (history_size <= 0) {
            error("history_size: %zu\n", history_size);
            error("History file is empty.\n");
            util_close(&history);
            return;
        }
        if (history_size >= INT32_MAX) {
            error("History file is too big.\n");
            error("Max size is %d bytes.", INT32_MAX);
            return;
        }
    }

    history_map = mmap(NULL, history_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
                       history.fd, 0);

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

    while ((left > 0) && (p = memchr(begin, TEXT_TAG, (ulong)left))) {
        Entry *e;
        char type = *(p + 1);
        *p = '\0';

        e = &entries[history_length];
        e->content_length = (int32)(p - begin);

        if (type == IMAGE_TAG) {
            e->trimmed = 0;
            e->trimmed_length = (int16)e->content_length;
            is_image[history_length] = true;
            e->content = util_memdup(begin, (usize)(e->content_length + 1));
        } else {
            int32 size;
            if (e->content_length >= TRIMMED_SIZE) {
                size = e->content_length + 1 + TRIMMED_SIZE + 1;
            } else {
                size = (e->content_length + 1)*2;
            }
            e->content = xmalloc((usize)size);
            memcpy(e->content, begin, (usize)(e->content_length + 1));

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

    if (munmap(history_map, history_size) < 0) {
        error("Error unmapping %p with %zu bytes: %s\n", (void *)history_map,
              history_size, strerror(errno));
    }
    util_close(&history);
    return;
}

int32
history_repeated_index(const char *content, const int32 length) {
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
        if (!memcmp(e->content, content, (usize)length)) {
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
    time_t t = time(NULL);
    int32 file;
    isize w;
    isize copied = 0;
    char image_file[256];
    int32 n;

    n = SNPRINTF(image_file, "%s/%ld.png", tmp_directory, t);

    if ((file
         = open(image_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR))
        < 0) {
        error("Error opening %s for saving: %s\n", image_file, strerror(errno));
        return -1;
    }

    do {
        w = write(file, *(content + copied), (usize)*length);
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
    memcpy(*content, image_file, (usize)*length + 1);
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
        free(content);
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
        e->content = xmalloc((usize)size);
        memcpy(e->content, content, (usize)(e->content_length + 1));

        content_trim_spaces(&(e->trimmed), &(e->trimmed_length), e->content,
                            e->content_length);
        is_image[history_length] = false;
        break;
    case CLIPBOARD_IMAGE:
        e->trimmed = 0;
        e->trimmed_length = (int16)e->content_length;
        e->content = util_memdup(content, (usize)(length + 1));
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

        memcpy(&entries[0], &entries[HISTORY_KEEP_SIZE],
               HISTORY_KEEP_SIZE*sizeof(*entries));
        memset(&entries[HISTORY_KEEP_SIZE], 0,
               HISTORY_KEEP_SIZE*sizeof(*entries));

        memcpy(&is_image[0], &is_image[HISTORY_KEEP_SIZE],
               HISTORY_KEEP_SIZE*sizeof(*is_image));
        memset(&is_image[HISTORY_KEEP_SIZE], 0,
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
        if (pipe(fd)) {
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
        memmove(&entries[id], &(entries[id + 1]),
                (usize)(history_length - id)*sizeof(*entries));
        memset(&entries[history_length - 1], 0, sizeof(*entries));
    }
    history_length -= 1;

    return;
}

void
history_reorder(const int32 oldindex) {
    DEBUG_PRINT("%d", oldindex)
    Entry aux = entries[oldindex];
    bool aux2 = is_image[oldindex];

    memmove(&entries[oldindex], &entries[oldindex + 1],
            (usize)(history_length - oldindex)*sizeof(*entries));
    memmove(&entries[history_length - 1], &aux, sizeof(*entries));

    memmove(&is_image[oldindex], &is_image[oldindex + 1],
            (usize)(history_length - oldindex)*sizeof(*is_image));
    memmove(&is_image[history_length - 1], &aux2, sizeof(*is_image));
    return;
}

void
history_free_entry(const Entry *e, int32 index) {
    DEBUG_PRINT("{\n    %s,\n    %d\n}", e->content, e->content_length)
    length_counts[e->content_length] -= 1;

    if (is_image[index]) {
        unlink(e->content);
    }
    free(e->content);

    return;
}

#endif /* HISTORY_C */
