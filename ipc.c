// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(IPC_C)
#define IPC_C

#include "clipsim.h"
#include "cbase/util.c"
#include "history.c"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_ipc 1
#elif !defined(TESTING_ipc)
#define TESTING_ipc 0
#endif

#define IPC_SOCKET_TIMEOUT_SECONDS 5

typedef struct IpcRequest {
    int32 command;
    int32 id;
} IpcRequest;

static char ipc_directory[PATH_MAX];
static char ipc_socket_name[PATH_MAX];
static File ipc_socket = {.file = NULL, .fd = -1, .name = ipc_socket_name};

static void ipc_daemon_history_save(int32);
static void ipc_client_check_save(int32 *);
static void ipc_daemon_pipe_entries(int32);
static void ipc_daemon_pipe_id(int32, int32);
static bool ipc_write_all(int32, void *, int64, char *);
static bool ipc_read_all(int32, void *, int64, char *);
static bool ipc_daemon_dprintf(int32, char *, char *, ...)
    __attribute__((format(printf, 3, 4)));
static void ipc_client_print_entries(int32 *);
static void ipc_resolve_socket_name(void);
static void ipc_make_directory(void);
static bool ipc_set_socket_timeout(int32, char *);
static int32 ipc_connect_socket(bool);
static void ipc_make_socket(void);
static void ipc_clean_socket(void);

static void *ipc_daemon_listen(void *) __attribute__((noreturn));
static void ipc_client_speak(int32, int32);

void
ipc_resolve_socket_name(void) {
    static bool resolved = false;
    char *XDG_RUNTIME_DIR;
    int32 n;

    if (resolved) {
        return;
    }

    GETENV(XDG_RUNTIME_DIR);
    if ((XDG_RUNTIME_DIR != NULL) && (XDG_RUNTIME_DIR[0] != '\0')) {
        n = SNPRINTF(ipc_directory, "%s/clipsim", XDG_RUNTIME_DIR);
        if ((n > 0) && (n < (int32)SIZEOF(ipc_directory))) {
            n = SNPRINTF(ipc_socket_name, "%s/daemon.sock", ipc_directory);
            if ((n > 0)
                && (n < (int32)SIZEOF(((struct sockaddr_un *)0)->sun_path))) {
                resolved = true;
                return;
            }
        }
    }

    n = SNPRINTF(ipc_directory, "/tmp/clipsim-%lu", (unsigned long)getuid());
    if ((n <= 0) || (n >= (int32)SIZEOF(ipc_directory))) {
        error("Error resolving ipc directory name.\n");
        fatal(EXIT_FAILURE);
    }

    n = SNPRINTF(ipc_socket_name, "%s/daemon.sock", ipc_directory);
    if ((n <= 0)
        || (n >= (int32)SIZEOF(((struct sockaddr_un *)0)->sun_path))) {
        error("Error resolving ipc socket name.\n");
        fatal(EXIT_FAILURE);
    }

    resolved = true;
    return;
}

void
ipc_make_directory(void) {
    struct stat st;

    ipc_resolve_socket_name();

    if (mkdir(ipc_directory, 0700) < 0) {
        if (errno != EEXIST) {
            error("Error creating %s: %s\n",
                  ipc_directory, strerror(errno));
            fatal(EXIT_FAILURE);
        }
    }

    if (stat(ipc_directory, &st) < 0) {
        error("Error checking %s: %s\n",
              ipc_directory, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    if (!S_ISDIR(st.st_mode)) {
        error("%s is not a directory.\n", ipc_directory);
        fatal(EXIT_FAILURE);
    }
    if (chmod(ipc_directory, 0700) < 0) {
        error("Error setting permissions on %s: %s.\n",
              ipc_directory, strerror(errno));
    }

    return;
}

bool
ipc_set_socket_timeout(int32 fd, char *name) {
    struct timeval timeout;

    timeout.tv_sec = IPC_SOCKET_TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        error("Error setting receive timeout on %s: %s.\n",
              name, strerror(errno));
        return false;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        error("Error setting send timeout on %s: %s.\n",
              name, strerror(errno));
        return false;
    }

    return true;
}

bool
ipc_write_all(int32 fd, void *data, int64 size, char *name) {
    int64 written = 0;

    if (size < 0) {
        error("Error writing negative length to %s.\n", name);
        return false;
    }

    while (written < size) {
        int64 w;

        errno = 0;
        w = write64(fd, (char *)data + written, size - written);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EPIPE) {
                error("Peer closed %s before clipsim finished writing.\n",
                      name);
            } else if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                error("Timed out writing to %s.\n", name);
            } else {
                error("Error writing to %s: %s.\n", name, strerror(errno));
            }
            return false;
        }
        if (w == 0) {
            error("Error writing to %s: short write.\n", name);
            return false;
        }
        written += w;
    }
    return true;
}

bool
ipc_read_all(int32 fd, void *data, int64 size, char *name) {
    int64 bytes_read = 0;

    if (size < 0) {
        error("Error reading negative length from %s.\n", name);
        return false;
    }

    while (bytes_read < size) {
        int64 r;

        errno = 0;
        r = read64(fd, (char *)data + bytes_read, size - bytes_read);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                error("Timed out reading from %s.\n", name);
            } else {
                error("Error reading from %s: %s.\n", name, strerror(errno));
            }
            return false;
        }
        if (r == 0) {
            error("Peer closed %s before sending a complete request.\n", name);
            return false;
        }
        bytes_read += r;
    }
    return true;
}

bool
ipc_daemon_dprintf(int32 fd, char *name, char *format, ...) {
    char buffer[BUFSIZ];
    int32 n;
    va_list args;

    va_start(args, format);
    n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if ((n < 0) || (n >= (int32)SIZEOF(buffer))) {
        error("Error formatting daemon ipc response.\n");
        return false;
    }

    return ipc_write_all(fd, buffer, n, name);
}

void *
ipc_daemon_listen(void *unused) {
    DEBUG_PRINT("void")
    int32 client_fd;

    (void)unused;
    ipc_make_socket();

    while (true) {
        IpcRequest request;
        socklen_t size;
        struct sockaddr_un client_addr;

        if (DEBUGGING) {
            error("ipc_daemon_listen loop...\n");
        }

        memset64(&client_addr, 0, sizeof(client_addr));
        size = sizeof(client_addr);
        client_fd = accept(ipc_socket.fd, (struct sockaddr *)&client_addr,
                           &size);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            error("Error accepting connection on %s: %s.\n",
                  ipc_socket.name, strerror(errno));
            continue;
        }

        ipc_set_socket_timeout(client_fd, ipc_socket.name);
        if (!ipc_read_all(client_fd, &request, sizeof(request),
                          ipc_socket.name)) {
            XCLOSE(&client_fd, ipc_socket.name);
            continue;
        }

        xpthread_mutex_lock(&lock);

        switch (request.command) {
        case COMMAND_PRINT:
            ipc_daemon_pipe_entries(client_fd);
            break;
        case COMMAND_SAVE:
            ipc_daemon_history_save(client_fd);
            break;
        case COMMAND_COPY:
            history_recover(request.id);
            break;
        case COMMAND_REMOVE:
            history_remove(request.id);
            break;
        case COMMAND_INFO:
            ipc_daemon_pipe_id(client_fd, request.id);
            break;
        default:
            error("Invalid command received: '%c'\n", request.command);
            break;
        }

        xpthread_mutex_unlock(&lock);
        XCLOSE(&client_fd, ipc_socket.name);
    }
}

void
ipc_client_speak(int32 command, int32 id) {
    DEBUG_PRINT("%u, %d", command, id)
    IpcRequest request;
    int32 fd;

    fd = ipc_connect_socket(false);
    ipc_set_socket_timeout(fd, ipc_socket.name);

    request.command = command;
    request.id = id;
    if (!ipc_write_all(fd, &request, sizeof(request), ipc_socket.name)) {
        XCLOSE(&fd, ipc_socket.name);
        fatal(EXIT_FAILURE);
    }

    switch (command) {
    case COMMAND_PRINT:
        ipc_client_print_entries(&fd);
        break;
    case COMMAND_SAVE:
        ipc_client_check_save(&fd);
        break;
    case COMMAND_COPY:
    case COMMAND_REMOVE:
        break;
    case COMMAND_INFO:
        ipc_client_print_entries(&fd);
        break;
    default:
        error("Invalid command: %d\n", command);
        XCLOSE(&fd, ipc_socket.name);
        exit(EXIT_FAILURE);
    }

    XCLOSE(&fd, ipc_socket.name);
    return;
}

void
ipc_daemon_history_save(int32 fd) {
    DEBUG_PRINT("%d", fd)
    char saved;
    int64 saved_size = sizeof(*(&saved));

    error("Trying to save history...\n");
    saved = (char)history_save();

    ipc_write_all(fd, &saved, saved_size, ipc_socket.name);
    return;
}

void
ipc_client_check_save(int32 *fd) {
    DEBUG_PRINT("%d", *fd)
    char saved = 0;

    error("Trying to save history...\n");

    if (ipc_read_all(*fd, &saved, sizeof(*(&saved)), ipc_socket.name)) {
        if (saved) {
            error("History saved to disk.\n");
        } else {
            error("Error saving history to disk.\n");
        }
    } else {
        error("Error reading saving result from daemon.\n");
    }

    XCLOSE(fd, ipc_socket.name);
    if (!saved) {
        exit(EXIT_FAILURE);
    }
    return;
}

void
ipc_daemon_pipe_entries(int32 fd) {
    DEBUG_PRINT("%d", fd)

    if (history_length <= 0) {
        error("Clipboard history empty. Start copying text.\n");
        return;
    }

    for (int32 i = history_length - 1; i >= 0; i -= 1) {
        Entry *e = &entries[i];
        int64 size = e->trimmed_length + 1;
        char *trimmed = &e->content[e->trimmed];

        if (!ipc_daemon_dprintf(fd, ipc_socket.name,
                                "%.*d ", PRINT_DIGITS, i)) {
            break;
        }
        if (!ipc_write_all(fd, trimmed, size, ipc_socket.name)) {
            break;
        }
    }

    return;
}

void
ipc_daemon_pipe_id(int32 fd, int32 id) {
    DEBUG_PRINT("%d, %d", fd, id)
    Entry *e;
    int64 tag_size = sizeof(*(&IMAGE_TAG));

    if (history_length <= -1) {
        error("Clipboard history empty. Start copying text.\n");
        ipc_daemon_dprintf(fd, ipc_socket.name,
                           "000 Clipboard history empty. "
                           "Start copying text.\n");
        return;
    }

    if (id < 0) {
        id = history_length + id;
    }
    if ((id >= history_length) || (id < 0)) {
        error("Invalid index: %d\n", id);
        return;
    }

    e = &entries[id];
    if (is_image[id]) {
        if (!ipc_write_all(fd, &IMAGE_TAG, tag_size, ipc_socket.name)) {
            return;
        }
    } else {
        if (!ipc_daemon_dprintf(fd, ipc_socket.name,
                                "Length: \033[31;1m%d\n\033[0;m",
                                e->content_length)) {
            return;
        }
    }
    ipc_write_all(fd, e->content, e->content_length, ipc_socket.name);
    return;
}

void
ipc_client_print_entries(int32 *fd) {
    DEBUG_PRINT("%d", *fd)
    static char buffer[BUFSIZ];
    int64 r;

    r = read64(*fd, buffer, sizeof(buffer));
    if (r <= 0) {
        error("Error reading data from %s", ipc_socket.name);
        if (r < 0) {
            error(": %s", strerror(errno));
        }
        error(".\n");

        XCLOSE(fd, ipc_socket.name);
        exit(EXIT_FAILURE);
    }

    if (buffer[0] != IMAGE_TAG) {
        do {
            fwrite64(buffer, 1, r, stdout);
        } while ((r = read64(*fd, buffer, sizeof(buffer))) > 0);
        if (r < 0) {
            error("Error reading data from %s: %s.\n",
                  ipc_socket.name, strerror(errno));
            XCLOSE(fd, ipc_socket.name);
            exit(EXIT_FAILURE);
        }
    } else {
        int32 test;
        int64 image_path_length = r - 1;
        char *CLIPSIM_IMAGE_PREVIEW;

        if (r == 1) {
            r = read64(*fd, buffer + 1, sizeof(buffer) - 1);
            if (r <= 0) {
                error("Error reading image name from %s.\n", ipc_socket.name);
                goto close;
            }
            image_path_length = r;
        }
        if (image_path_length >= ((int64)sizeof(buffer) - 1)) {
            error("Image path from %s is too long.\n", ipc_socket.name);
            goto close;
        }
        buffer[image_path_length + 1] = '\0';

        XCLOSE(fd, ipc_socket.name);
        if ((test = open(buffer + 1, O_RDONLY)) >= 0) {
            XCLOSE(&test, buffer + 1);
        } else {
            error("Error opening %s: %s\n", buffer + 1, strerror(errno));
            return;
        }

        CLIPSIM_IMAGE_PREVIEW = getenv("CLIPSIM_IMAGE_PREVIEW");
        if (CLIPSIM_IMAGE_PREVIEW == NULL) {
            CLIPSIM_IMAGE_PREVIEW = "chafa";
        }
        if (!strcmp(CLIPSIM_IMAGE_PREVIEW, "stiv_draw")) {
            execlp("stiv_draw", "stiv_draw", buffer + 1, "30", "15", NULL);
            error("Error executing stiv_draw: %s.\n", strerror(errno));
        } else {
            execlp("chafa", "chafa", buffer + 1, "-s", "40x", NULL);
            error("Error executing chafa: %s.\n", strerror(errno));
        }
    }

close:
    XCLOSE(fd, ipc_socket.name);
    return;
}

int32
ipc_connect_socket(bool quiet) {
    struct sockaddr_un addr;
    int32 fd;

    ipc_resolve_socket_name();
    memset64(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy64(addr.sun_path, ipc_socket.name, strlen32(ipc_socket.name) + 1);

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        if (!quiet) {
            error("Error creating ipc socket: %s.\n", strerror(errno));
        }
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (!quiet) {
            error("Could not connect to clipsim daemon at %s. "
                  "Is `clipsim --daemon` running?\n", ipc_socket.name);
        }
        XCLOSE(&fd, ipc_socket.name);
        return -1;
    }

    return fd;
}

void
ipc_make_socket(void) {
    struct sockaddr_un addr;
    int32 existing;

    ipc_make_directory();

    existing = ipc_connect_socket(true);
    if (existing >= 0) {
        XCLOSE(&existing, ipc_socket.name);
        error("clipsim --daemon is already running at %s.\n",
              ipc_socket.name);
        fatal(EXIT_FAILURE);
    }

    ipc_clean_socket();

    if ((ipc_socket.fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        error("Error creating ipc socket: %s\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }

    memset64(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy64(addr.sun_path, ipc_socket.name, strlen32(ipc_socket.name) + 1);

    if (bind(ipc_socket.fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        error("Error binding ipc socket %s: %s\n",
              ipc_socket.name, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    if (listen(ipc_socket.fd, 64) < 0) {
        error("Error listening on ipc socket %s: %s\n",
              ipc_socket.name, strerror(errno));
        fatal(EXIT_FAILURE);
    }

    return;
}

void
ipc_clean_socket(void) {
    DEBUG_PRINT("%s", ipc_socket.name)
    if (unlink(ipc_socket.name) < 0) {
        if (errno != ENOENT) {
            error("Error deleting %s: %s\n",
                  ipc_socket.name, strerror(errno));
            fatal(EXIT_FAILURE);
        }
    }
    return;
}

#if TESTING_ipc
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
    ipc_resolve_socket_name();
    ipc_make_directory();
    ipc_clean_socket();
    return 0;
}
#endif

#endif /* IPC_C */
