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

#include "clipsim.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <ftw.h>

#define MAX_OPEN_FD 64

static volatile bool recovered = false;
static int32 history_length;
static File history = { .file = NULL, .fd = -1, .name = NULL };
static char *XDG_CACHE_HOME = NULL;
static uint8 length_counts[ENTRY_MAX_LENGTH] = {0};
static char *directory = "/tmp/clipsim";

static int32 history_callback_delete(const char *,
                                     const struct stat *, int32, struct FTW *);
static int32 history_repeated_index(const char *, const int32);
static void history_clean(void);
static void history_free_entry(const Entry *, int32);
static void history_reorder(const int32);
static void history_save_entry(Entry *, int32);
static void history_save_image(char **, int32 *);

int32
history_length_get(void) {
    DEBUG_PRINT("void");
    return history_length;
}

int32
history_callback_delete(const char *path,
                        const struct stat *stat,
                        int32 typeflag,
                        struct FTW *ftwbuf) {
    (void) stat;
    (void) ftwbuf;

    if (typeflag == FTW_F) {
        if (unlink(path) < 0)
            error("Error deleting %s: %s.\n", path, strerror(errno));
    } else if (typeflag == FTW_DP) {
        if (rmdir(path) < 0)
            error("Error deleting %s: %s.\n", path, strerror(errno));
    }


    return 0;
}

void
history_delete_tmp(void) {
    error("Deleting images...");

    nftw(directory, history_callback_delete, MAX_OPEN_FD, FTW_DEPTH | FTW_PHYS);

    return;
}

void history_backup(void) {
    char buffer[PATH_MAX];
    int32 n = snprintf(buffer, sizeof(buffer), "%s.bak", history.name);
    if (n <= 0) {
        error("Error in snprintf.\n");
        exit(EXIT_FAILURE);
    }
    if (rename(history.name, buffer) < 0) {
        error("Error creating backup history file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return;
}

void
history_save_entry(Entry *e, int32 index) {
    DEBUG_PRINT("{\n    %s,\n    %d\n}",
                e->content, e->content_length);
    char image_save[PATH_MAX];
    usize tag_size = sizeof(*(&IMAGE_TAG));
    isize w;

    if (is_image[index]) {
        int32 n;
        char *base = basename(e->content);
        n = snprintf(image_save, sizeof(image_save), 
                     "%s/clipsim/%s", XDG_CACHE_HOME, base);
        if (n <= 0) {
            error("Error printing image path.\n");
            return;
        }

        if (strcmp(image_save, e->content)) {
            if (util_copy_file(image_save, e->content) < 0) {
                error("Error copying %s to %s: %s.\n", 
                      e->content, image_save, strerror(errno));
                history_remove(index);
                return;
            }
        }
        if (write(history.fd, image_save, (usize) n) < n) {
            error("Error writing %s: %s\n", image_save, strerror(errno));
            history_remove(index);
            return;
        }
        if (write(history.fd, &IMAGE_TAG, tag_size) < (isize) tag_size) {
            error("Error writing IMAGE_TAG: %s\n", strerror(errno));
            history_remove(index);
            return;
        }
    } else {
        int32 left = e->content_length;
        int32 offset = 0;
        do {
            w = write(history.fd, e->content + offset, (usize) left);
            left -= w;
            offset += w;
            if (left == 0)
                break;
        } while (w > 0);
        if (w < 0) {
            error("Error writing %s: %s\n", e->content, strerror(errno));
            history_remove(index);
            return;
        }
        if (write(history.fd, &TEXT_TAG, tag_size) < (isize) tag_size) {
            error("Error writing TEXT_TAG: %s\n", strerror(errno));
            history_remove(index);
            return;
        }
    }
    return;
}

bool
history_save(void) {
    DEBUG_PRINT("void");
    int32 saved;

    if (history_length <= 0) {
        error("History is empty. Not saving.\n");
        return false;
    }
    if (history.name == NULL) {
        error("History file name unresolved, can't save history.");
        return false;
    }
    if ((history.fd = open(history.name, O_WRONLY | O_CREAT | O_TRUNC,
                                         S_IRUSR | S_IWUSR)) < 0) {
        error("Error opening history file for saving: %s\n", strerror(errno));
        return false;
    }

    for (int32 i = 0; i < history_length; i += 1)
        history_save_entry(&entries[i], i);

    if ((saved = fsync(history.fd)) < 0)
        error("Error saving history to disk: %s\n", strerror(errno));
    else
        error("History saved to disk.\n");
    util_close(&history);
    return saved >= 0;
}

void history_exit(int32 unused) {
    (void) unused;

    history_save();
    history_delete_tmp();
    _exit(EXIT_SUCCESS);
}

void
history_read(void) {
    DEBUG_PRINT("void");
    usize history_size;
    char *history_map;
    char *begin;

    const char *clipsim = "clipsim/history";
    usize length;

    if ((XDG_CACHE_HOME = getenv("XDG_CACHE_HOME")) == NULL) {
        error("XDG_CACHE_HOME needs to be set.\n");
        exit(EXIT_FAILURE);
    }

    length = strlen(XDG_CACHE_HOME);
    length += 1 + strlen(clipsim);
    if (length > (PATH_MAX - 1)) {
        error("XDG_CACHE_HOME is too long.\n");
        exit(EXIT_FAILURE);
    }

    {
        char buffer[PATH_MAX];
        int32 n = snprintf(buffer, sizeof(buffer), "%s/%s",
                                                  XDG_CACHE_HOME, clipsim);
        if (n < (int32) length)
            util_die_notify("Error printing to buffer: %s\n", strerror(errno));
        buffer[sizeof(buffer) - 1] = '\0';

        usize size = (usize) n + 1;
        history.name = util_memdup(buffer, size);

        char *clipsim_dir = dirname(buffer);
        if (mkdir(clipsim_dir, 0770) < 0) {
            if (errno != EEXIST) {
                util_die_notify("Error creating dir %s: %s\n",
                                clipsim_dir, strerror(errno));
            }
        }
    }

    history_length = 0;
    if ((history.fd = open(history.name, O_RDONLY)) < 0) {
        error("Error opening history file for reading: %s\n"
              "History will start empty.\n", strerror(errno));
        return;
    }

    {
        struct stat history_stat;
        if (fstat(history.fd, &history_stat) < 0) {
            error("Error getting file information: %s\n"
                  "History will start empty.\n", strerror(errno));
            util_close(&history);
            return;
        }
        history_size = (usize) history_stat.st_size;
        if (history_size <= 0) {
            error("history_size: %zu\n", history_size);
            error("History file is empty.\n");
            util_close(&history);
            return;
        }
    }

    history_map = mmap(NULL, history_size, 
                       PROT_READ | PROT_WRITE, MAP_PRIVATE,
                       history.fd, 0);

    if (history_map == MAP_FAILED) {
        error("Error mapping history file to memory: %s"
              "History will start empty.\n", strerror(errno));
        util_close(&history);
        return;
    }

    begin = history_map;
    for (char *p = history_map; p < history_map + history_size; p += 1) {
        Entry *e;
        char c;

        if ((*p == TEXT_TAG) || (*p == IMAGE_TAG)) {
            c = *p;
            *p = '\0';

            e = &entries[history_length];
            e->content_length = (int32) (p - begin);

            if (c == IMAGE_TAG) {
                e->trimmed = 0;
                e->trimmed_length = e->content_length;
                is_image[history_length] = true;
                e->content = util_memdup(begin, (usize)e->content_length + 1);
            } else {
                usize size;
                if (e->content_length >= TRIMMED_SIZE) {
                    size = e->content_length + 1 + TRIMMED_SIZE + 1;
                } else {
                    size = (e->content_length + 1)*2;
                }
                e->content = util_memdup(begin, size);
                content_trim_spaces(&e->trimmed, &e->trimmed_length, 
                                     e->content, e->content_length);
                is_image[history_length] = false;
            }
            begin = p + 1;

            length_counts[e->content_length] += 1;
            history_length += 1;

            if (history_length >= HISTORY_BUFFER_SIZE)
                break;
        }
    }

    if (munmap(history_map, history_size) < 0) {
        error("Error unmapping %p with %zu bytes: %s\n",
              (void *) history_map, history_size, strerror(errno));
    }
    util_close(&history);
    return;
}

int32
history_repeated_index(const char *content, const int32 length) {
    DEBUG_PRINT("%s, %d", content, length);
    int32 candidates = length_counts[length];
    if (candidates == 0)
        return -1;
    for (int32 i = history_length - 1; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        if (e->content_length != length)
            continue;
        if (!memcmp(e->content, content, (usize) length))
            return i;

        candidates -= 1;
        if (candidates <= 0)
            return -1;
    }
    return -1;
}

void
history_save_image(char **content, int32 *length) {
    DEBUG_PRINT("%p, %d", (void *) content, *length);
    time_t t = time(NULL);
    int32 fp;
    isize w = 0;
    isize copied = 0;
    int32 n;
    char buffer[256];

    n = snprintf(buffer, sizeof(buffer), "%s/%ld.png", directory, t);
    if (n < (int32) strlen(directory))
        util_die_notify("Error printing image path.\n");

    buffer[sizeof(buffer) - 1] = '\0';
    if ((fp = open(buffer, O_WRONLY | O_CREAT | O_TRUNC,
                                      S_IRUSR | S_IWUSR)) < 0) {
        util_die_notify("Error opening image file for saving: "
                        "%s\n", strerror(errno));
    }

    do {
        w = write(fp, *(content + copied), (usize) *length);
        if (w <= 0)
            break;
        copied += w;
        *length -= w;
    } while (*length > 0);
    if (w < 0) {
        error("Error writing to %s: %s\n", buffer, strerror(errno));
        exit(EXIT_FAILURE);
    }

    *length = n;
    memcpy(*content, buffer, (usize) *length + 1);
    return;
}

void
history_append(char *content, int32 length) {
    DEBUG_PRINT("%s, %d", content, length);
    int32 oldindex;
    int32 kind;
    usize size;
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

    kind = content_check_content((uchar *) content, length);
    switch (kind) {
    case CLIPBOARD_TEXT:
        content_remove_newline(content, &length);
        break;
    case CLIPBOARD_IMAGE:
        history_save_image(&content, &length);
        break;
    default:
        return;
    }

    if ((oldindex = history_repeated_index(content, length)) >= 0) {
        error("Entry is equal to previous entry. Reordering...\n");
        if (oldindex != (history_length - 1))
            history_reorder(oldindex);
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
        e->content = util_memdup(content, size);

        content_trim_spaces(&(e->trimmed), &(e->trimmed_length), 
                            e->content, e->content_length);
        is_image[history_length] = false;
        break;
    case CLIPBOARD_IMAGE:
        e->trimmed = 0;
        e->trimmed_length = e->content_length;
        e->content = util_memdup(content, length+1);
        is_image[history_length] = true;
        break;
    default:
        error("Unexpected default case.\n");
        exit(EXIT_FAILURE);
    }
    XFree(content);

    history_length += 1;
    if (history_length >= HISTORY_BUFFER_SIZE)
        history_clean();

    return;
}

void
history_recover(int32 id) {
    DEBUG_PRINT("%d", id);
    pid_t child;
    int32 fd[2];
    Entry *e;
    bool istext;
    char *xclip = "xclip";
    char *xclip_path = "/usr/bin/xclip";

    if (history_length <= 0) {
        error("Clipboard history empty. Start copying text.\n");
        return;
    }
    if (id < 0)
        id = history_length + id;
    if ((id >= history_length) || (id < 0)) {
        error("Invalid index for recovery: %d\n", id);
        recovered = true;
        return;
    }

    e = &entries[id];
    istext = (is_image[id] == false);
    if (istext) {
        if (pipe(fd))
            util_die_notify("Error creating pipe: %s\n", strerror(errno));
    }

    switch ((child = fork())) {
    case 0:
        if (istext) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            execl(xclip_path, xclip, "-selection", "clipboard", NULL);
        } else {
            execl(xclip_path, xclip, "-selection", "clipboard",
                              "-target", "image/png", e->content, NULL);
        }
        util_die_notify("Error in exec(%s): %s", xclip_path, strerror(errno));
    case -1:
        util_die_notify("Error in fork(%s): %s", xclip_path, strerror(errno));
    default:
        if (istext && (close(fd[0]) < 0))
            util_die_notify("Error closing pipe 0: %s\n", strerror(errno));
    }

    if (istext) {
        dprintf(fd[1], "%s", e->content);
        if (close(fd[1]) < 0) {
            util_die_notify("Error closing pipe 1: %s\n", strerror(errno));
        }
    }
    if (wait(NULL) < 0)
        util_die_notify("Error waiting for fork: %s\n", strerror(errno));

    if (id != (history_length - 1))
        history_reorder(id);

    recovered = true;
    return;
}

void
history_remove(int32 id) {
    DEBUG_PRINT("%d", id);
    if (history_length <= 0)
        return;

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
                (usize) (history_length - id)*sizeof(*entries));
        memset(&entries[history_length - 1], 0, sizeof(*entries));
    }
    history_length -= 1;

    return;
}

void
history_reorder(const int32 oldindex) {
    DEBUG_PRINT("%d", oldindex);
    Entry aux = entries[oldindex];
    bool aux2 = is_image[oldindex];

    memmove(&entries[oldindex], &entries[oldindex + 1],
            (usize) (history_length - oldindex)*sizeof(*entries));
    memmove(&entries[history_length - 1], &aux, sizeof(*entries));

    memmove(&is_image[oldindex], &is_image[oldindex + 1],
            (usize) (history_length - oldindex)*sizeof(*is_image));
    memmove(&is_image[history_length - 1], &aux2, sizeof(*is_image));
    return;
}

void
history_free_entry(const Entry *e, int32 index) {
    DEBUG_PRINT("{\n    %s,\n    %d\n}",
                e->content, e->content_length);
    length_counts[e->content_length] -= 1;

    if (is_image[index])
        unlink(e->content);
    free(e->content);

    return;
}

void
history_clean(void) {
    DEBUG_PRINT("void");
    for (int32 i = 0; i < HISTORY_KEEP_SIZE; i += 1)
        history_free_entry(&entries[i], i);

    memcpy(&entries[0], &entries[HISTORY_KEEP_SIZE],
           HISTORY_KEEP_SIZE * sizeof(*entries));
    memset(&entries[HISTORY_KEEP_SIZE], 0,
           HISTORY_KEEP_SIZE * sizeof(*entries));

    memcpy(&is_image[0], &is_image[HISTORY_KEEP_SIZE],
           HISTORY_KEEP_SIZE * sizeof(*is_image));
    memset(&is_image[HISTORY_KEEP_SIZE], 0,
           HISTORY_KEEP_SIZE * sizeof(*is_image));

    history_length = HISTORY_KEEP_SIZE;
    return;
}
