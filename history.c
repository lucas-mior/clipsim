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

#include "clipsim.h"
#include <stdlib.h>

static volatile bool recovered = false;
static int32 lastindex;
static File history = { .file = NULL, .fd = -1, .name = NULL };
static char *XDG_CACHE_HOME = NULL;
static uint8 length_counts[ENTRY_MAX_LENGTH] = {0};

static int32 history_repeated_index(const char *, const size_t);
static void history_reorder(const int32);
static void history_free_entry(const Entry *);
static void history_clean(void);
static void history_save_image(char **, ulong *);
static void history_save_entry(Entry *);

int32 history_lastindex(void) {
    return lastindex;
}

void history_save_entry(Entry *e) {
    char image_save[PATH_MAX];
    if (e->image_path) {
        int length;
        length = snprintf(image_save, sizeof(image_save), 
                          "%s/clipsim/%s", XDG_CACHE_HOME, basename(e->image_path));
        if (strcmp(image_save, e->image_path)) {
            if (util_copy_file(image_save, e->image_path) < 0) {
                fprintf(stderr, "Error copying %s to %s: %s.\n", 
                                 e->image_path, image_save, strerror(errno));
                return;
            }
        }
        if (write(history.fd, image_save, (size_t) length) < 0) {
            fprintf(stderr, "Error writing %s: %s\n", image_save, strerror(errno));
            return;
        }
        if (write(history.fd, &IMAGE_END, sizeof(IMAGE_END)) < 0) {
            fprintf(stderr, "Error writing IMAGE_END: %s\n", strerror(errno));
            return;
        }
    } else {
        if (write(history.fd, e->content, e->content_length) < 0) {
            fprintf(stderr, "Error writing %s: %s\n", e->content, strerror(errno));
            return;
        }
        if (write(history.fd, &TEXT_END, sizeof(IMAGE_END)) < 0) {
            fprintf(stderr, "Error writing TEXT_END: %s\n", strerror(errno));
            return;
        }
    }
}

bool history_save(void) {
    DEBUG_PRINT("history_save(void)\n")
    int saved;

    if (lastindex < 0) {
        fprintf(stderr, "History is empty. Not saving.\n");
        return false;
    }
    if (history.name == NULL) {
        fprintf(stderr, "History file name unresolved, can't save history.");
        return false;
    }
    if ((history.fd = open(history.name, O_WRONLY | O_CREAT | O_TRUNC,
                                         S_IRUSR | S_IWUSR)) < 0) {
        fprintf(stderr, "Failed to open history file for saving: "
                        "%s\n", strerror(errno));
        return false;
    }

    for (uint i = 0; i <= (uint) lastindex; i += 1)
        history_save_entry(&entries[i]);

    if ((saved = fsync(history.fd)) < 0)
        fprintf(stderr, "Error saving history to disk: %s\n", strerror(errno));
    else
        fprintf(stderr, "History saved to disk.\n");
    util_close(&history);
    return saved >= 0;
}

void history_read(void) {
    DEBUG_PRINT("history_read(void)\n")
    struct stat history_stat;
    static char buffer[PATH_MAX];
    size_t history_length;
    char *history_content;
    char *begin;

    const char *clipsim = "clipsim/history";
    size_t length;

    if ((XDG_CACHE_HOME = getenv("XDG_CACHE_HOME")) == NULL) {
        fprintf(stderr, "XDG_CACHE_HOME needs to be set.\n");
        exit(EXIT_FAILURE);
    }

    length = strlen(XDG_CACHE_HOME);
    length += 1 + strlen(clipsim);
    if (length > (PATH_MAX - 1)) {
        fprintf(stderr, "XDG_CACHE_HOME is too long.\n");
        exit(EXIT_FAILURE);
    }

    (void) snprintf(buffer, sizeof(buffer), "%s/%s", XDG_CACHE_HOME, clipsim);
    history.name = buffer;

    lastindex = -1;
    if ((history.fd = open(history.name, O_RDONLY)) < 0) {
        fprintf(stderr, "Error opening history file for reading: %s\n"
                        "History will start empty.\n", strerror(errno));
        return;
    }

    if (fstat(history.fd, &history_stat) < 0) {
        fprintf(stderr, "Error getting file information: %s\n"
                        "History will start empty.\n", strerror(errno));
        util_close(&history);
        return;
    }
    history_length = (size_t) history_stat.st_size;
    if (history_length <= 0) {
        fprintf(stderr, "History_length: %zu\n", history_length);
        fprintf(stderr, "History file is empty.\n");
        util_close(&history);
        return;
    }

    history_content = mmap(NULL, history_length, 
                           PROT_READ | PROT_WRITE, MAP_PRIVATE, history.fd, 0);
    if (history_content == MAP_FAILED) {
        fprintf(stderr, "Error mapping history file to memory: %s"
                        "History will start empty.\n", strerror(errno));
        util_close(&history);
        return;
    }

    begin = history_content;
    for (char *p = history_content; p < history_content + history_length; p++) {
        Entry *e;
        char c;

        if ((*p == TEXT_END) || (*p == IMAGE_END)) {
            c = *p;
            *p = '\0';

            lastindex += 1;
            e = &entries[lastindex];
            e->content_length = (size_t) (p - begin);
            e->content = util_realloc(NULL, e->content_length+1);
            memcpy(e->content, begin, e->content_length+1);

            if (c == IMAGE_END) {
                e->trimmed = e->content;
                e->image_path = e->content;
                e->trimmed_length = e->content_length;
            } else {
                content_trim_spaces(&e->trimmed, &e->trimmed_length, 
                                     e->content, e->content_length);
                e->image_path = NULL;
            }
            begin = p+1;

            length_counts[e->content_length] += 1;

            if (lastindex > (int32) HISTORY_BUFFER_SIZE)
                break;
        }
    }

    munmap(history_content, history_length);
    util_close(&history);
    return;
}

int32 history_repeated_index(const char *content, const size_t length) {
    DEBUG_PRINT("history_repeated_index(%.*s, %lu)\n", 20, content, length)
    if (length_counts[length] == 0)
        return -1;
    for (int32 i = lastindex; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        if (e->content_length == length) {
            if (!strcmp(e->content, content))
                return i;
        }
    }
    return -1;
}

void history_save_image(char **content, ulong *length) {
    time_t t = time(NULL);
    int fp;
    ssize_t w = 0;
    size_t copied = 0;
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "/tmp/clipsim/%lu.png", t);
    buffer[sizeof(buffer)-1] = '\0';
    if ((fp = open(buffer, O_WRONLY | O_CREAT | O_TRUNC,
                                      S_IRUSR | S_IWUSR)) < 0) {
        fprintf(stderr, "Failed to open image file for saving: "
                        "%s\n", strerror(errno));
        return;
    }

    do {
        w = write(fp, *(content + copied), *length);
        if (w <= 0)
            break;
        copied += (size_t) w;
        *length -= (size_t) w;
    } while (*length > 0);
    *length = strlen(buffer);
    *content = util_realloc(*content, *length+1);
    memcpy(*content, buffer, *length+1);
    return;
}

void history_append(char *content, ulong length) {
    DEBUG_PRINT("history_append(%.*s, %lu)\n", 20, content, length)
    int32 oldindex;
    int32 kind;
    Entry *e;

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
        fprintf(stderr, "Entry is equal to previous entry. Reordering...\n");
        if (oldindex != lastindex)
            history_reorder(oldindex);
        free(content);
        return;
    }

    lastindex += 1;
    e = &entries[lastindex];
    e->content = content;
    e->content_length = length;
    length_counts[length] += 1;

    switch (kind) {
    case CLIPBOARD_TEXT:
        content_trim_spaces(&(e->trimmed), &(e->trimmed_length), 
                            e->content, e->content_length);
        e->image_path = NULL;
        break;
    case CLIPBOARD_IMAGE:
        e->trimmed = e->content;
        e->trimmed_length = e->content_length;
        e->image_path = e->content;
        break;
    default:
        break;
    }

    if (lastindex+1 >= (int32) HISTORY_BUFFER_SIZE) {
        history_clean();
        history_save();
    }

    return;
}

void history_recover(int32 id) {
    DEBUG_PRINT("history_recover(%d)", id)
    pid_t child = -1;
    int fd[2];
    Entry *e;
    bool istext;

    if (lastindex < 0) {
        fprintf(stderr, "Clipboard history empty. Start copying text.\n");
        return;
    }
    if (id < 0)
        id = lastindex + id + 1;
    if (id > lastindex) {
        fprintf(stderr, "Invalid index: %d\n", id);
        recovered = true;
        return;
    }

    e = &entries[id];
    istext = (e->image_path == NULL);
    if (istext) {
        if (pipe(fd)){
            fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
            return;
        }
    }

    switch ((child = fork())) {
    case 0:
        if (istext) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            execlp("/usr/bin/xsel", "xsel", "-b", NULL);
        } else {
            execlp("/usr/bin/xclip", "xclip", "-selection", "clipboard",
                   "-target", "image/png", e->image_path, NULL);
        }
        fprintf(stderr, "Failed to exec(): %s", strerror(errno));
        return;
    case -1:
        fprintf(stderr, "Failed to fork(): %s", strerror(errno));
        return;
    default:
        if (istext)
            close(fd[0]);
    }

    if (istext) {
        dprintf(fd[1], "%s", e->content);
        close(fd[1]);
    }
    wait(NULL);

    if (id != lastindex)
        history_reorder(id);

    recovered = true;
    return;
}

void history_remove(int32 id) {
    DEBUG_PRINT("history_remove(%d)\n", id)
    if (lastindex <= 0)
        return;

    if (id < 0) {
        id = lastindex + id + 1;
    } else if (id == lastindex) {
        history_recover(-2);
        history_remove(-2);
        return;
    }
    if (id > lastindex) {
        fprintf(stderr, "Invalid index %d for deletion.\n", id);
        return;
    }

    history_free_entry(&entries[id]);

    if (id < lastindex) {
        memmove(&entries[id], &entries[id+1],
                (size_t) (lastindex - id)*sizeof(Entry));
        memset(&entries[lastindex], 0, sizeof(Entry));
    }
    lastindex -= 1;

    return;
}

void history_reorder(const int32 oldindex) {
    DEBUG_PRINT("history_reorder(%d)\n", oldindex)
    Entry aux = entries[oldindex];
    memmove(&entries[oldindex], &entries[oldindex+1],
            (size_t) (lastindex - oldindex)*sizeof(Entry));
    memmove(&entries[lastindex], &aux, sizeof(Entry));
    return;
}

void history_free_entry(const Entry *e) {
    length_counts[e->content_length] -= 1;
    if (e->image_path)
        unlink(e->image_path);
    free(e->content);
    // image_path does not have to be freed
    // because e->content is the same pointer
    if (e->trimmed != e->content)
        free(e->trimmed);
    return;
}

void history_clean(void) {
    DEBUG_PRINT("history_clean(void)\n")
    for (uint i = 0; i <= HISTORY_KEEP_SIZE-1; i += 1)
        history_free_entry(&entries[i]);

    memcpy(&entries[0], &entries[HISTORY_KEEP_SIZE],
           HISTORY_KEEP_SIZE*sizeof(Entry));
    memset(&entries[HISTORY_KEEP_SIZE], 0, HISTORY_KEEP_SIZE*sizeof(Entry));
    lastindex = HISTORY_KEEP_SIZE-1;
    return;
}
