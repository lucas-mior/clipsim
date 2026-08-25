// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(COMMAND_C)
#define COMMAND_C

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_command 1
#elif !defined(TESTING_command)
#define TESTING_command 0
#endif

#include "cbase.h"

void
command_result_init(CommandResult *result) {
    *result = (CommandResult){0};

    result->pid = -1;
    result->status = -1;
    result->stdin_fd = -1;
    result->stdout_fd = -1;
    result->stderr_fd = -1;

    return;
}

void
command_error_set(Command *command, int32 error_status) {
    command->error_status = error_status;
    command->result.error_status = error_status;
    return;
}

static int32
command_error_return(Command *command) {
    int32 err;

    err = command->error_status;
    if (err == 0) {
        err = EIO;
    }
    return -err;
}

enum CommandFlag
command_flags_normalized(enum CommandFlag flags) {
    if (flags & COMMAND_MERGE_STDERR) {
        if (flags & COMMAND_CAPTURE_STDERR) {
            flags |= COMMAND_CAPTURE_STDOUT;
        }
    }
    return flags;
}

bool
command_flags_capture(enum CommandFlag flags) {
    return flags & (COMMAND_CAPTURE_STDOUT | COMMAND_CAPTURE_STDERR);
}

#if OS_UNIX
int32
command_status_from_wait(int status, CommandResult *result) {
    result->exited = false;
    result->signaled = false;
    result->exit_status = -1;
    result->term_signal = 0;

    if (WIFEXITED(status)) {
        result->exited = true;
        result->exit_status = WEXITSTATUS(status);
        result->status = result->exit_status;
    } else if (WIFSIGNALED(status)) {
        result->signaled = true;
        result->term_signal = WTERMSIG(status);
        result->status = 128 + result->term_signal;
    } else {
        result->status = 127;
    }

    return result->status;
}

#endif

void
command_result_file_descriptors_close(CommandResult *result) {
    if (result->pid == 0) {
        result->stdin_fd = -1;
        result->stdout_fd = -1;
        result->stderr_fd = -1;
        return;
    }

    XCLOSE(&result->stdin_fd);
    XCLOSE(&result->stdout_fd);
    XCLOSE(&result->stderr_fd);

    return;
}

void
command_result_free(CommandResult *result) {
    command_result_file_descriptors_close(result);

    free2(result->output, result->output_len + 1);
    free2(result->stdout_output, result->stdout_len + 1);
    free2(result->stderr_output, result->stderr_len + 1);

    command_result_init(result);
    return;
}

void
command_result_append(
    StrBuilder *output,
    StrBuilder *stdout_output,
    StrBuilder *stderr_output,
    bool is_stderr,
    char *data,
    int32 data_len
) {
    if (data_len <= 0) {
        return;
    }

    sb_append(output, data, data_len);
    if (is_stderr) {
        sb_append(stderr_output, data, data_len);
    } else {
        sb_append(stdout_output, data, data_len);
    }

    return;
}

#if OS_WINDOWS
typedef struct CommandWindowsCaptureFile {
    HANDLE handle;
    uint32 padding;
    char path[PATH_MAX];
} CommandWindowsCaptureFile;

static void
command_windows_capture_file_init(CommandWindowsCaptureFile *capture) {
    *capture = (CommandWindowsCaptureFile){0};
    capture->handle = INVALID_HANDLE_VALUE;
    return;
}

static void
command_windows_error_set(Command *command, DWORD error_code) {
    windows_set_errno(error_code);
    command_error_set(command, (int32)error_code);
    return;
}

static bool
command_windows_capture_file_open(
    Command *command,
    CommandWindowsCaptureFile *capture,
    char *prefix
) {
    char temp_dir[PATH_MAX];
    SECURITY_ATTRIBUTES security_attributes = {0};
    DWORD temp_dir_len;

    temp_dir_len = GetTempPathA((DWORD)SIZEOF(temp_dir), temp_dir);
    if ((temp_dir_len == 0) || (temp_dir_len >= (DWORD)SIZEOF(temp_dir))) {
        command_windows_error_set(command, GetLastError());
        return false;
    }

    if (GetTempFileNameA(temp_dir, prefix, 0, capture->path) == 0) {
        command_windows_error_set(command, GetLastError());
        return false;
    }

    security_attributes.nLength = (DWORD)SIZEOF(security_attributes);
    security_attributes.lpSecurityDescriptor = NULL;
    security_attributes.bInheritHandle = TRUE;

    capture->handle = CreateFileA(capture->path,
                                  GENERIC_WRITE,
                                  FILE_SHARE_READ |FILE_SHARE_WRITE,
                                  &security_attributes,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_TEMPORARY,
                                  NULL);
    if (capture->handle == INVALID_HANDLE_VALUE) {
        command_windows_error_set(command, GetLastError());
        DeleteFileA(capture->path);
        capture->path[0] = '\0';
        return false;
    }

    return true;
}

static bool
command_windows_capture_file_close(Command *command,
                                   CommandWindowsCaptureFile *capture) {
    if (capture->handle == INVALID_HANDLE_VALUE) {
        return true;
    }

    if (!CloseHandle(capture->handle)) {
        command_windows_error_set(command, GetLastError());
        capture->handle = INVALID_HANDLE_VALUE;
        return false;
    }

    capture->handle = INVALID_HANDLE_VALUE;
    return true;
}

static void
command_windows_capture_file_cleanup(CommandWindowsCaptureFile *capture) {
    if (capture->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(capture->handle);
    }
    if (capture->path[0] != '\0') {
        DeleteFileA(capture->path);
    }
    command_windows_capture_file_init(capture);
    return;
}

static bool
command_windows_capture_file_read(Command *command,
                                  CommandWindowsCaptureFile *capture,
                                  char **output,
                                  int32 *output_len) {
    int32 read_len;

    if (!command_windows_capture_file_close(command, capture)) {
        return false;
    }

    if ((read_len = read_entire_file(capture->path, output)) < 0) {
        command_error_set(command, -read_len);
        return false;
    }

    *output_len = read_len;
    return true;
}

static bool
command_windows_result_read_captured(
    Command *command,
    enum CommandFlag flags,
    CommandWindowsCaptureFile *stdout_capture,
    CommandWindowsCaptureFile *stderr_capture
) {
    StrBuilder output = {0};
    char *stdout_output = NULL;
    char *stderr_output = NULL;
    int32 stdout_len = 0;
    int32 stderr_len = 0;

    if ((flags & COMMAND_CAPTURE_STDOUT)
        && !command_windows_capture_file_read(command,
                                             stdout_capture,
                                             &stdout_output,
                                             &stdout_len)) {
        return false;
    }

    if (flags & COMMAND_CAPTURE_STDERR) {
        if (flags & COMMAND_MERGE_STDERR) {
            stderr_output = xstrndup(STRLIT(""));
            stderr_len = 0;
        } else if (!command_windows_capture_file_read(command,
                                                      stderr_capture,
                                                      &stderr_output,
                                                      &stderr_len)) {
            free2(stdout_output, stdout_len + 1);
            return false;
        }
    }

    if (command_flags_capture(flags)) {
        if (flags & COMMAND_CAPTURE_STDOUT) {
            sb_append(&output, stdout_output, stdout_len);
        }
        if ((flags & COMMAND_CAPTURE_STDERR)
            && !(flags & COMMAND_MERGE_STDERR)) {
            sb_append(&output, stderr_output, stderr_len);
        }
        command->result.output = sb_steal_exact(&output,
                                                &command->result.output_len);
    }

    if (flags & COMMAND_CAPTURE_STDOUT) {
        command->result.stdout_output = stdout_output;
        command->result.stdout_len = stdout_len;
        stdout_output = NULL;
        stdout_len = 0;
    }
    if (flags & COMMAND_CAPTURE_STDERR) {
        command->result.stderr_output = stderr_output;
        command->result.stderr_len = stderr_len;
        stderr_output = NULL;
        stderr_len = 0;
    }

    free2(stdout_output, stdout_len + 1);
    free2(stderr_output, stderr_len + 1);
    return true;
}

char *
command_windows_argv0(
    Command *command,
    char *argv0_windows,
    int32 *argv0_len
) {
    char *exe = ".exe";
    int64 exe_len = strlen32(exe);
    int64 len0 = strlen32(command->argv[0]);

    if (len0 >= BUFSIZ) {
        error("Invalid arguments.\n");
        fatal(EXIT_FAILURE);
    }

    *argv0_len = (int32)len0;
    if (memmem64(command->argv[0], len0 + 1, exe, exe_len + 1) != NULL) {
        return command->argv[0];
    }

    if ((len0 + exe_len) >= BUFSIZ) {
        error("Invalid arguments.\n");
        fatal(EXIT_FAILURE);
    }
    memcpy64(argv0_windows, command->argv[0], len0);
    memcpy64(argv0_windows + len0, exe, exe_len + 1);
    *argv0_len = (int32)(len0 + exe_len);
    return argv0_windows;
}

void
command_windows_command_line(
    Command *command,
    char *cmdline,
    int64 cmdline_len
) {
    char argv0_windows[BUFSIZ];
    int64 j = 0;

    for (int32 i = 0; i < command->argc; i += 1) {
        char *argument;
        int32 argument_len;
        bool needs_quotes;

        if (i == 0) {
            argument = command_windows_argv0(command,
                                             argv0_windows,
                                             &argument_len);
        } else {
            argument = command->argv[i];
            argument_len = command->argvs_lens[i];
        }

        needs_quotes = argument_len == 0;

        for (int32 k = 0; k < argument_len; k += 1) {
            if ((argument[k] == ' ') || (argument[k] == '\t')) {
                needs_quotes = true;
                break;
            }
        }

        if (needs_quotes) {
            if ((j + argument_len + 3) >= cmdline_len) {
                error("Command line is too long.\n");
                fatal(EXIT_FAILURE);
            }

            cmdline[j] = '"';
            memcpy64(&cmdline[j + 1], argument, argument_len);
            cmdline[j + argument_len + 1] = '"';
            j += argument_len + 2;
        } else {
            if ((j + argument_len + 1) >= cmdline_len) {
                error("Command line is too long.\n");
                fatal(EXIT_FAILURE);
            }

            memcpy64(&cmdline[j], argument, argument_len);
            j += argument_len;
        }
        if (i < command->argc - 1) {
            cmdline[j++] = ' ';
        }
    }
    cmdline[j] = '\0';
    return;
}

int32
command_windows_run_process(Command *command, enum CommandFlag flags) {
    char cmdline[BUFSIZ] = {0};
    CommandWindowsCaptureFile stdout_capture;
    CommandWindowsCaptureFile stderr_capture;
    PROCESS_INFORMATION proc_info = {0};
    STARTUPINFO startup_info = {0};
    DWORD exit_code = 0;
    BOOL inherit_handles = TRUE;
    BOOL success;

    command_windows_capture_file_init(&stdout_capture);
    command_windows_capture_file_init(&stderr_capture);
    flags = command_flags_normalized(flags);

    if ((flags & COMMAND_STDIN_TTY)
        && (freopen("CONIN$", "r", stdin) == NULL)) {
        command_error_set(command, errno);
        return -1;
    }

    if ((flags & COMMAND_CAPTURE_STDOUT)
        && !command_windows_capture_file_open(command,
                                             &stdout_capture,
                                             "cos")) {
        return -1;
    }
    if ((flags & COMMAND_CAPTURE_STDERR)
        && !(flags & COMMAND_MERGE_STDERR)
        && !command_windows_capture_file_open(command,
                                             &stderr_capture,
                                             "ces")) {
        command_windows_capture_file_cleanup(&stdout_capture);
        return -1;
    }

    command_windows_command_line(command, cmdline, SIZEOF(cmdline));

    startup_info.cb = sizeof(startup_info);
    if (command_flags_capture(flags) || (flags & COMMAND_STDIN_TTY)) {
        startup_info.dwFlags |= STARTF_USESTDHANDLES;
        startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        inherit_handles = TRUE;

#if CBASE_CRT_MSVC
        if (flags & COMMAND_STDIN_TTY) {
            startup_info.hStdInput = (HANDLE)_get_osfhandle(STDIN_FILENO);
        }
#endif
        if (flags & COMMAND_CAPTURE_STDOUT) {
            startup_info.hStdOutput = stdout_capture.handle;
        }
        if (flags & COMMAND_CAPTURE_STDERR) {
            if (flags & COMMAND_MERGE_STDERR) {
                startup_info.hStdError = stdout_capture.handle;
            } else {
                startup_info.hStdError = stderr_capture.handle;
            }
        }
    }

    success = CreateProcessA(NULL,
                             cmdline,
                             NULL,
                             NULL,
                             inherit_handles,
                             0,
                             NULL,
                             command->cwd,
                             &startup_info,
                             &proc_info);
    if (!success) {
        DWORD err = GetLastError();

        command_windows_capture_file_cleanup(&stdout_capture);
        command_windows_capture_file_cleanup(&stderr_capture);
        command_windows_error_set(command, err);
        error("Error running '%s': %llu.\n", cmdline, (ullong)err);
        if (err == ERROR_PATH_NOT_FOUND) {
            error("Path not found.\n");
        }
        return -1;
    }

    command->result.pid = proc_info.dwProcessId;

    if (WaitForSingleObject(proc_info.hProcess, INFINITE) != WAIT_OBJECT_0) {
        command_windows_error_set(command, GetLastError());
        CloseHandle(proc_info.hThread);
        CloseHandle(proc_info.hProcess);
        command_windows_capture_file_cleanup(&stdout_capture);
        command_windows_capture_file_cleanup(&stderr_capture);
        return -1;
    }

    if (!GetExitCodeProcess(proc_info.hProcess, &exit_code)) {
        command_windows_error_set(command, GetLastError());
        CloseHandle(proc_info.hThread);
        CloseHandle(proc_info.hProcess);
        command_windows_capture_file_cleanup(&stdout_capture);
        command_windows_capture_file_cleanup(&stderr_capture);
        return -1;
    }

    if (!CloseHandle(proc_info.hThread)) {
        command_windows_error_set(command, GetLastError());
        CloseHandle(proc_info.hProcess);
        command_windows_capture_file_cleanup(&stdout_capture);
        command_windows_capture_file_cleanup(&stderr_capture);
        return -1;
    }

    if (!CloseHandle(proc_info.hProcess)) {
        command_windows_error_set(command, GetLastError());
        command_windows_capture_file_cleanup(&stdout_capture);
        command_windows_capture_file_cleanup(&stderr_capture);
        return -1;
    }

    if (command_flags_capture(flags)
        && !command_windows_result_read_captured(command,
                                                flags,
                                                &stdout_capture,
                                                &stderr_capture)) {
        command_windows_capture_file_cleanup(&stdout_capture);
        command_windows_capture_file_cleanup(&stderr_capture);
        return -1;
    }

    command_windows_capture_file_cleanup(&stdout_capture);
    command_windows_capture_file_cleanup(&stderr_capture);
    return (int32)exit_code;
}
#endif

#if OS_UNIX
static bool
command_pipe_set_nonblock(int32 fd) {
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }

    return true;
}

static void
command_result_close_poll_fd(struct pollfd *pipe, int32 *fd, int32 *left) {
    if (*fd >= 0) {
        XCLOSE(fd);
    }
    pipe->fd = -1;
    pipe->events = 0;
    pipe->revents = 0;
    *left -= 1;
    return;
}

static void
command_result_process_stdin_event(
    Command *command,
    struct pollfd *pipe,
    int32 *left,
    int64 *stdin_offset
) {
    int64 bytes_written;
    int64 chunk_len;
    int64 left_to_write;

    if (pipe->revents & (POLLERR | POLLHUP | POLLNVAL)) {
        command_result_close_poll_fd(pipe, &command->result.stdin_fd, left);
        return;
    }
    if (!(pipe->revents & POLLOUT)) {
        pipe->revents = 0;
        return;
    }

    left_to_write = command->stdin_buffer_len - *stdin_offset;
    if (left_to_write <= 0) {
        command_result_close_poll_fd(pipe, &command->result.stdin_fd, left);
        return;
    }

    chunk_len = left_to_write;
    if (chunk_len > 65536) {
        chunk_len = 65536;
    }

    bytes_written = write64(command->result.stdin_fd,
                            command->stdin_buffer + *stdin_offset,
                            chunk_len);
    if (bytes_written > 0) {
        *stdin_offset += bytes_written;
        if (*stdin_offset >= command->stdin_buffer_len) {
            command_result_close_poll_fd(pipe,
                                         &command->result.stdin_fd,
                                         left);
        } else {
            pipe->revents = 0;
        }
        return;
    }

    if (bytes_written < 0) {
        if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            pipe->revents = 0;
            return;
        }
        if (errno == EPIPE) {
            command_result_close_poll_fd(pipe,
                                         &command->result.stdin_fd,
                                         left);
            return;
        }
        command_error_set(command, errno);
        error("Error writing child stdin: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }

    pipe->revents = 0;
    return;
}

static void
command_result_process_output_event(
    Command *command,
    struct pollfd *pipe,
    int32 *fd,
    int32 *left,
    StrBuilder *output,
    StrBuilder *stdout_output,
    StrBuilder *stderr_output,
    bool is_stderr
) {
    char buffer[4096];
    int64 read_bytes;

    errno = 0;
    read_bytes = read64(pipe->fd, buffer, SIZEOF(buffer));
    if (read_bytes > 0) {
        if (read_bytes >= MAXOF(command->result.output_len)) {
            error("Command output is too long.\n");
            fatal(EXIT_FAILURE);
        }
        command_result_append(output,
                              stdout_output,
                              stderr_output,
                              is_stderr,
                              buffer,
                              (int32)read_bytes);
        pipe->revents = 0;
        return;
    }
    if (read_bytes < 0) {
        if (errno == EINTR) {
            pipe->revents = 0;
            return;
        }
        command_error_set(command, errno);
        error("Error reading child output: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    }

    command_result_close_poll_fd(pipe, fd, left);
    return;
}

void
command_result_process_io(Command *command, enum CommandFlag flags) {
    enum {
        COMMAND_STDOUT_PIPE_INDEX = 0,
        COMMAND_STDERR_PIPE_INDEX = 1,
        COMMAND_STDIN_PIPE_INDEX = 2,
        COMMAND_PIPE_COUNT = 3,
    };
    struct pollfd pipes[COMMAND_PIPE_COUNT] = {0};
    StrBuilder output = {0};
    StrBuilder stdout_output = {0};
    StrBuilder stderr_output = {0};
    int32 nfds = 0;
    int32 left = 0;
    int64 stdin_offset = 0;
    void (*previous_sigpipe)(int) = NULL;
    bool sigpipe_changed = false;

    flags = command_flags_normalized(flags);
    for (int32 i = 0; i < COMMAND_PIPE_COUNT; i += 1) {
        pipes[i].fd = -1;
    }

    if (command->result.stdout_fd >= 0) {
        pipes[COMMAND_STDOUT_PIPE_INDEX].fd = command->result.stdout_fd;
        pipes[COMMAND_STDOUT_PIPE_INDEX].events = POLLIN;
        nfds = COMMAND_STDOUT_PIPE_INDEX + 1;
        left += 1;
    }
    if (command->result.stderr_fd >= 0) {
        pipes[COMMAND_STDERR_PIPE_INDEX].fd = command->result.stderr_fd;
        pipes[COMMAND_STDERR_PIPE_INDEX].events = POLLIN;
        nfds = COMMAND_STDERR_PIPE_INDEX + 1;
        left += 1;
    }
    if (command->result.stdin_fd >= 0) {
        if (command->stdin_buffer_len <= 0) {
            XCLOSE(&command->result.stdin_fd);
        } else {
            pipes[COMMAND_STDIN_PIPE_INDEX].fd = command->result.stdin_fd;
            pipes[COMMAND_STDIN_PIPE_INDEX].events = POLLOUT;
            nfds = COMMAND_STDIN_PIPE_INDEX + 1;
            left += 1;
            previous_sigpipe = signal(SIGPIPE, SIG_IGN);
            sigpipe_changed = true;
        }
    }

    while (left > 0) {
        int32 ready;

        ready = poll(pipes, (nfds_t)nfds, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            command_error_set(command, errno);
            error("Error polling child IO: %s.\n", strerror(errno));
            fatal(EXIT_FAILURE);
        }

        for (int32 i = 0; (i < nfds) && (ready > 0); i += 1) {
            if (pipes[i].fd < 0) {
                continue;
            }
            if (!pipes[i].revents) {
                continue;
            }

            ready -= 1;
            if (i == COMMAND_STDIN_PIPE_INDEX) {
                command_result_process_stdin_event(command,
                                                   &pipes[i],
                                                   &left,
                                                   &stdin_offset);
                continue;
            }

            if (i == COMMAND_STDERR_PIPE_INDEX) {
                command_result_process_output_event(command,
                                                    &pipes[i],
                                                    &command->result.stderr_fd,
                                                    &left,
                                                    &output,
                                                    &stdout_output,
                                                    &stderr_output,
                                                    true);
            } else {
                command_result_process_output_event(command,
                                                    &pipes[i],
                                                    &command->result.stdout_fd,
                                                    &left,
                                                    &output,
                                                    &stdout_output,
                                                    &stderr_output,
                                                    false);
            }
        }
    }

    if (sigpipe_changed) {
        signal(SIGPIPE, previous_sigpipe);
    }

    if (command_flags_capture(flags)) {
        command->result.output = sb_steal_exact(&output,
                                                &command->result.output_len);
    } else {
        sb_free(&output);
    }
    if (flags & COMMAND_CAPTURE_STDOUT) {
        command->result.stdout_output = sb_steal_exact(
            &stdout_output,
            &command->result.stdout_len);
    } else {
        sb_free(&stdout_output);
    }
    if (flags & COMMAND_CAPTURE_STDERR) {
        command->result.stderr_output = sb_steal_exact(
            &stderr_output,
            &command->result.stderr_len);
    } else {
        sb_free(&stderr_output);
    }
    return;
}

void
command_result_read_captured(Command *command) {
    command_result_process_io(command,
                              COMMAND_CAPTURE_STDOUT
                              |COMMAND_CAPTURE_STDERR);
    return;
}

void
command_child_env_apply(Command *command) {
    for (int32 i = 0; i < command->env_len; i += 1) {
        putenv(command->env[i]);
    }
    return;
}

void
command_child_exec(
    Command *command,
    enum CommandFlag flags,
    int stdin_pipe[2],
    int stdout_pipe[2],
    int stderr_pipe[2]
) {
    char *command_text;
    int32 command_text_len;

    if (command->cwd) {
        if (chdir(command->cwd) < 0) {
            error("Error changing directory to %s: %s.\n",
                  command->cwd, strerror(errno));
            _exit(127);
        }
    }

    if (flags & COMMAND_NEW_SESSION) {
        if (setsid() < 0) {
            error("Error in setsid: %s.\n", strerror(errno));
            _exit(127);
        }
    } else if (flags & COMMAND_NEW_PROCESS_GROUP) {
        if (setpgid(0, 0) < 0) {
            error("Error in setpgid: %s.\n", strerror(errno));
            _exit(127);
        }
    }

    if (stdin_pipe) {
        XCLOSE(&stdin_pipe[1]);
        xdup2(stdin_pipe[0], STDIN_FILENO);
    } else if (flags & COMMAND_STDIN_TTY) {
        if (!freopen("/dev/tty", "r", stdin)) {
            error("Error reopening stdin: %s.\n", strerror(errno));
        }
    } else if (flags & COMMAND_CLOSE_STDIN) {
        int stdin_fd = STDIN_FILENO;

        XCLOSE(&stdin_fd);
    }

    if (flags & COMMAND_CAPTURE_STDOUT) {
        XCLOSE(&stdout_pipe[0]);
        xdup2(stdout_pipe[1], STDOUT_FILENO);
    }

    if (flags & COMMAND_CAPTURE_STDERR) {
        if (flags & COMMAND_MERGE_STDERR) {
            xdup2(stdout_pipe[1], STDERR_FILENO);
        } else {
            XCLOSE(&stderr_pipe[0]);
            xdup2(stderr_pipe[1], STDERR_FILENO);
        }
    }

    if (stdin_pipe) {
        XCLOSE(&stdin_pipe[0]);
    }
    if (flags & COMMAND_CAPTURE_STDOUT) {
        XCLOSE(&stdout_pipe[1]);
    }
    if ((flags & COMMAND_CAPTURE_STDERR)
        && !(flags & COMMAND_MERGE_STDERR)) {
        XCLOSE(&stderr_pipe[1]);
    }

    command_child_env_apply(command);
    command_text = command_str(command, &command_text_len);
    if (DEBUGGING && !TESTING_command) {
        error2("Running %s \n", command_text);
    }

    execvp(command->argv[0], command->argv);
    error("Error executing\n%s\n%s.\n", command_text, strerror(errno));
    free2(command_text, command_text_len + 1);
    _exit(127);
}

int32
command_start(Command *command, enum CommandFlag flags) {
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    pid_t pid;

    flags = command_flags_normalized(flags);
    command_result_free(&command->result);
    command->error_status = 0;

    if (command->argc <= 0) {
        command_error_set(command, EINVAL);
        return command_error_return(command);
    }
    if ((flags & COMMAND_DETACHED) && command_flags_capture(flags)) {
        command_error_set(command, EINVAL);
        return command_error_return(command);
    }
    if ((command->stdin_buffer != NULL)
        && (flags & (COMMAND_ASYNC
                     |COMMAND_DETACHED
                     |COMMAND_STDIN_TTY
                     |COMMAND_CLOSE_STDIN))) {
        command_error_set(command, EINVAL);
        return command_error_return(command);
    }

    if (command->stdin_buffer != NULL) {
        xpipe(stdin_pipe);
        if (!command_pipe_set_nonblock(stdin_pipe[1])) {
            command_error_set(command, errno);
            XCLOSE(&stdin_pipe[0]);
            XCLOSE(&stdin_pipe[1]);
            return command_error_return(command);
        }
    }
    if (flags & COMMAND_CAPTURE_STDOUT) {
        xpipe(stdout_pipe);
    }
    if ((flags & COMMAND_CAPTURE_STDERR)
        && !(flags & COMMAND_MERGE_STDERR)) {
        xpipe(stderr_pipe);
    }

    switch (pid = fork()) {
    case -1:
        if (stdin_pipe[0] >= 0) {
            XCLOSE(&stdin_pipe[0]);
        }
        if (stdin_pipe[1] >= 0) {
            XCLOSE(&stdin_pipe[1]);
        }
        if (stdout_pipe[0] >= 0) {
            XCLOSE(&stdout_pipe[0]);
        }
        if (stdout_pipe[1] >= 0) {
            XCLOSE(&stdout_pipe[1]);
        }
        if (stderr_pipe[0] >= 0) {
            XCLOSE(&stderr_pipe[0]);
        }
        if (stderr_pipe[1] >= 0) {
            XCLOSE(&stderr_pipe[1]);
        }
        command_error_set(command, errno);
        error("Error forking: %s.\n", strerror(errno));
        fatal(EXIT_FAILURE);
    case 0:
        if (flags & COMMAND_DETACHED) {
            switch (fork()) {
            case -1:
                error("Error forking detached child: %s.\n", strerror(errno));
                _exit(127);
            case 0:
                break;
            default:
                _exit(0);
            }
        }
        if (command->stdin_buffer != NULL) {
            command_child_exec(command,
                               flags,
                               stdin_pipe,
                               stdout_pipe,
                               stderr_pipe);
        }
        command_child_exec(command, flags, NULL, stdout_pipe, stderr_pipe);
    default:
        break;
    }

    command->result.pid = (int64)pid;

    if (stdin_pipe[0] >= 0) {
        XCLOSE(&stdin_pipe[0]);
        command->result.stdin_fd = stdin_pipe[1];
    }
    if (flags & COMMAND_CAPTURE_STDOUT) {
        XCLOSE(&stdout_pipe[1]);
        command->result.stdout_fd = stdout_pipe[0];
    }
    if ((flags & COMMAND_CAPTURE_STDERR)
        && !(flags & COMMAND_MERGE_STDERR)) {
        XCLOSE(&stderr_pipe[1]);
        command->result.stderr_fd = stderr_pipe[0];
    }

    return 0;
}

int32
command_wait(Command *command) {
    int status;

    if (command->result.pid <= 0) {
        command_error_set(command, EINVAL);
        return command_error_return(command);
    }

    while (waitpid((pid_t)command->result.pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        command_error_set(command, errno);
        error("Error waiting for child: %s.\n", strerror(errno));
        return command_error_return(command);
    }

    command_status_from_wait(status, &command->result);
    return 0;
}

int32
command_signal(Command *command, int32 signal_number, bool process_group) {
    pid_t pid;

    if (command->result.pid <= 0) {
        command_error_set(command, EINVAL);
        return command_error_return(command);
    }

    pid = (pid_t)command->result.pid;
    if (process_group) {
        pid = -pid;
    }

    if (kill(pid, signal_number) < 0) {
        command_error_set(command, errno);
        error("Error sending signal %d to child: %s.\n",
              signal_number, strerror(errno));
        return command_error_return(command);
    }

    return 0;
}
#endif

int32
command_run(Command *command, enum CommandFlag flags) {
#if OS_UNIX
    int32 err;
#endif

    flags = command_flags_normalized(flags);

#if OS_UNIX
    if ((err = command_start(command, flags)) < 0) {
        return err;
    }
    if (flags & COMMAND_DETACHED) {
        if ((err = command_wait(command)) < 0) {
            return err;
        }
        return 0;
    }
    if (flags & COMMAND_ASYNC) {
        return 0;
    }
    if (command_flags_capture(flags) || (command->stdin_buffer != NULL)) {
        command_result_process_io(command, flags);
        if (command->error_status) {
            return command_error_return(command);
        }
    }
    if ((err = command_wait(command)) < 0) {
        return err;
    }
    return 0;
#elif OS_WINDOWS
    command_result_free(&command->result);
    command->error_status = 0;
    if (command->argc <= 0) {
        command_error_set(command, EINVAL);
        return -EINVAL;
    }
    if ((flags & COMMAND_ASYNC) || (command->stdin_buffer != NULL)) {
        command_error_set(command, ENOSYS);
        return -ENOSYS;
    }
    command->result.status = command_windows_run_process(command, flags);
    if (command->error_status) {
        return command_error_return(command);
    }
    command->result.exit_status = command->result.status;
    command->result.exited = true;
    return 0;
#else
    (void)flags;
    command_result_free(&command->result);
    command_error_set(command, ENOSYS);
    return -ENOSYS;
#endif
}

bool
command_run_sync(Command *command, int *exit_status) {
    int32 err;

    err = command_run(command, COMMAND_NONE);
    if ((err == 0) && exit_status) {
        *exit_status = command->result.status;
    }
    return err == 0;
}

bool
command_run_async(Command *command, enum CommandFlag flags) {
    flags |= COMMAND_ASYNC;
    return command_run(command, flags) == 0;
}

bool
command_run_capture(Command *command, enum CommandFlag flags) {
    flags |= COMMAND_CAPTURE_STDOUT;
    return command_run(command, flags) == 0;
}

bool
command_run_capture_all(Command *command) {
    return command_run(command,
                       COMMAND_CAPTURE_STDOUT
                       |COMMAND_CAPTURE_STDERR) == 0;
}

bool
command_run_capture_combined(Command *command) {
    return command_run(command,
                       COMMAND_CAPTURE_STDOUT
                       |COMMAND_CAPTURE_STDERR
                       |COMMAND_MERGE_STDERR) == 0;
}

void
command_print(Command *command) {
    printf(RED("%s"), command->argv[0]);
    for (int32 i = 1; i < command->argc; i += 1) {
        printf(" %s", command->argv[i]);
    }
    printf("\n");
    return;
}

char *
command_str(Command *command, int32 *len) {
    StrBuilder str_builder = {0};

    for (int32 i = 0; i < command->argc; i += 1) {
        if (i > 0) {
            sb_append_byte(&str_builder, ' ');
        }
        SB_APPEND(&str_builder, command->argv[i], command->argvs_lens[i]);
    }
    return sb_steal_exact(&str_builder, len);
}

void
command_vector_reserve(
    char ***items,
    int32 **item_lens,
    int32 *cap,
    int32 len,
    int32 extra
) {
    int32 needed;

    needed = len + extra + 1;
    if ((needed < len) || (needed >= MAXOF(*cap))) {
        error("Command has too many items.\n");
        fatal(EXIT_FAILURE);
    }
    if (*cap > needed) {
        return;
    }

    do {
        int32 oldcap = *cap;

        *cap += 16;
        *items = realloc2(*items, oldcap, *cap, SIZEOF(**items));
        *item_lens = realloc2(*item_lens, oldcap, *cap, SIZEOF(**item_lens));
    } while (*cap <= needed);

    return;
}

void
command_push_owned_length(
    char ***items,
    int32 **item_lens,
    int32 *len,
    int32 *cap,
    char *argument,
    int32 argument_len
) {
    char *copy;

    if (argument_len < 0) {
        error("Command argument has invalid length.\n");
        fatal(EXIT_FAILURE);
    }

    command_vector_reserve(items, item_lens, cap, *len, 1);
    copy = malloc2(argument_len + 1);
    memcpy64(copy, argument, argument_len);
    copy[argument_len] = '\0';

    (*items)[*len] = copy;
    (*item_lens)[*len] = argument_len;
    *len += 1;
    (*items)[*len] = NULL;
    (*item_lens)[*len] = 0;
    return;
}

void
command_push_length(Command *command, char *argument, int32 argument_len) {
    command_push_owned_length(&command->argv,
                              &command->argvs_lens,
                              &command->argc,
                              &command->cap,
                              argument,
                              argument_len);
    return;
}

static void
command_push(Command *command, char *argument) {
    command_push_length(command, argument, strlen32(argument));
    return;
}

void
command_push_array(Command *command, int32 argc, char **argv) {
    if (argv == NULL) {
        return;
    }
    for (int32 i = 0; (i < argc) && argv[i]; i += 1) {
        command_push(command, argv[i]);
    }
    return;
}

int32
command_stdin_buffer_set(Command *command, char *data, int64 data_len) {
    if (command == NULL) {
        return -EINVAL;
    }
    if (data_len < 0) {
        command_error_set(command, EINVAL);
        return command_error_return(command);
    }
    if (data == NULL) {
        command_error_set(command, EINVAL);
        return command_error_return(command);
    }

    command->stdin_buffer = data;
    command->stdin_buffer_len = data_len;
    return 0;
}

void
command_stdin_buffer_clear(Command *command) {
    if (command == NULL) {
        return;
    }

    command->stdin_buffer = NULL;
    command->stdin_buffer_len = 0;
    return;
}

void
command_env_push_length(
    Command *command,
    char *assignment,
    int32 assignment_len
) {
    command_push_owned_length(&command->env,
                              &command->env_lens,
                              &command->env_len,
                              &command->env_cap,
                              assignment,
                              assignment_len);
    return;
}

void
command_env_push(Command *command, char *assignment) {
    command_env_push_length(command, assignment, strlen32(assignment));
    return;
}

void
command_push_split(Command *command, char *arguments, char *delimiters) {
    char *argument = arguments;
    int32 delimiters_len = strlen32(delimiters);

    for (;;) {
        int64 argument_len;
        int32 argument_len32;
        char *argument_start;

        while ((*argument != '\0')
               && (memchr(delimiters, *argument,
                          (size_t)delimiters_len) != NULL)) {
            argument += 1;
        }
        if (*argument == '\0') {
            break;
        }

        argument_start = argument;
        while ((*argument != '\0')
               && (memchr64(delimiters, *argument, delimiters_len) == NULL)) {
            argument += 1;
        }
        argument_len = argument - argument_start;
        if (argument_len >= MAXOF(argument_len32)) {
            error("Command argument is too long.\n");
            fatal(EXIT_FAILURE);
        }
        argument_len32 = (int32)argument_len;
        command_push_length(command, argument_start, argument_len32);
    }
    return;
}

void
command_argv0_set(Command *command, char *argument) {
    int32 argument_len = strlen32(argument);

    ASSERT_POSITIVE(command->argc);
    free2(command->argv[0], command->argvs_lens[0] + 1);
    command->argv[0] = xstrdup(argument);
    command->argvs_lens[0] = argument_len;
    return;
}

void
command_cwd_clear(Command *command) {
    free2(command->cwd, command->cwd_len + 1);
    command->cwd = NULL;
    command->cwd_len = 0;
    return;
}

void
command_cwd_set(Command *command, char *cwd) {
    command_cwd_clear(command);
    if (cwd) {
        command->cwd_len = strlen32(cwd);
        command->cwd = xstrdup(cwd);
    }
    return;
}

void
command_reset(Command *command) {
    for (int32 i = 0; i < command->argc; i += 1) {
        free2(command->argv[i], command->argvs_lens[i] + 1);
        command->argv[i] = NULL;
        command->argvs_lens[i] = 0;
    }
    command->argc = 0;
    if (command->argv) {
        command->argv[0] = NULL;
    }
    if (command->argvs_lens) {
        command->argvs_lens[0] = 0;
    }
    command_error_set(command, 0);
    command_stdin_buffer_clear(command);
    command_result_free(&command->result);
    return;
}

void
command_env_clear(Command *command) {
    for (int32 i = 0; i < command->env_len; i += 1) {
        free2(command->env[i], command->env_lens[i] + 1);
        command->env[i] = NULL;
        command->env_lens[i] = 0;
    }
    command->env_len = 0;
    if (command->env) {
        command->env[0] = NULL;
    }
    if (command->env_lens) {
        command->env_lens[0] = 0;
    }
    return;
}

void
command_free(Command *command) {
    command_reset(command);
    command_env_clear(command);
    command_cwd_clear(command);

    free2(command->argv, command->cap*SIZEOF(*command->argv));
    free2(command->argvs_lens,
          command->cap*SIZEOF(*command->argvs_lens));
    free2(command->env, command->env_cap*SIZEOF(*command->env));
    free2(command->env_lens,
          command->env_cap*SIZEOF(*command->env_lens));

    command->argv = NULL;
    command->argvs_lens = NULL;
    command->env = NULL;
    command->env_lens = NULL;
    command->cap = 0;
    command->env_cap = 0;
    return;
}

void
command_printf(Command *command, char *fmt, ...) {
    va_list ap;
    va_list ap2;
    int32 n;
    char *argument;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        va_end(ap2);
        error("Error formatting \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }

    argument = malloc2(n + 1);
    n = vsnprintf(argument, (size_t)n + 1, fmt, ap2);
    va_end(ap2);

    command_push_length(command, argument, n);

    free2(argument, n + 1);
    return;
}

void
command_env_printf(Command *command, char *fmt, ...) {
    va_list ap;
    va_list ap2;
    int32 n;
    char *argument;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        va_end(ap2);
        error("Error formatting \"%s\".", fmt);
        fatal(EXIT_FAILURE);
    }

    argument = malloc2(n + 1);
    n = vsnprintf(argument, (size_t)n + 1, fmt, ap2);
    va_end(ap2);

    command_env_push_length(command, argument, n);

    free2(argument, n + 1);
    return;
}

#if TESTING_command
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(int argc, char **argv) {
    struct timespec t0;
    struct timespec t1;

    (void)argc;
    (void)argv;

    time_monotonic_precise(&t0);
    {
        Command cmd = {0};
        char *command_text;
        int32 len;

        COMMAND_PUSH(&cmd, "echo");
        command_printf(&cmd, "--val=%d", 123);
        COMMAND_PUSH(&cmd, "test");

        ASSERT_EQUAL(cmd.argc, 3);
        ASSERT_EQUAL(cmd.argv[0], "echo");
        ASSERT_EQUAL(cmd.argv[1], "--val=123");
        ASSERT_EQUAL(cmd.argv[2], "test");
        ASSERT_EQUAL(cmd.argvs_lens[0], 4);
        ASSERT_EQUAL(cmd.argvs_lens[1], 9);
        ASSERT_EQUAL(cmd.argvs_lens[2], 4);

        command_argv0_set(&cmd, "printf");
        ASSERT_EQUAL(cmd.argv[0], "printf");
        ASSERT_EQUAL(cmd.argvs_lens[0], 6);
        command_argv0_set(&cmd, "echo");

        command_text = command_str(&cmd, &len);
        ASSERT_EQUAL(command_text, "echo --val=123 test");
        free2(command_text, len + 1);
        command_print(&cmd);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);
        ASSERT(cmd.argv[0] == NULL);

        command_push_split(&cmd, "  alpha beta  gamma ", " ");
        ASSERT_EQUAL(cmd.argc, 3);
        ASSERT_EQUAL(cmd.argv[0], "alpha");
        ASSERT_EQUAL(cmd.argv[1], "beta");
        ASSERT_EQUAL(cmd.argv[2], "gamma");
        ASSERT(cmd.argv[cmd.argc] == NULL);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        COMMAND_PUSH(&cmd, "first", "second");
        ASSERT_EQUAL(cmd.argc, 2);
        ASSERT_EQUAL(cmd.argv[0], "first");
        ASSERT_EQUAL(cmd.argv[1], "second");
        ASSERT_EQUAL(cmd.argvs_lens[0], 5);
        ASSERT_EQUAL(cmd.argvs_lens[1], 6);
        ASSERT_NULL(cmd.argv[cmd.argc]);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        {
            enum {
                COMMAND_PART_ARGUMENT_COUNT = 64,
            };
            char terminal_arguments[COMMAND_PART_ARGUMENT_COUNT*2];
            char diff_arguments[COMMAND_PART_ARGUMENT_COUNT*2];

            for (int32 i = 0; i < COMMAND_PART_ARGUMENT_COUNT; i += 1) {
                terminal_arguments[i*2] = 't';
                terminal_arguments[i*2 + 1] = ' ';
                diff_arguments[i*2] = 'd';
                diff_arguments[i*2 + 1] = ' ';
            }
            terminal_arguments[SIZEOF(terminal_arguments) - 1] = '\0';
            diff_arguments[SIZEOF(diff_arguments) - 1] = '\0';

            command_push_split(&cmd, terminal_arguments, " ");
            COMMAND_PUSH(&cmd, "-e");
            command_push_split(&cmd, diff_arguments, " ");
            COMMAND_PUSH(&cmd, "/destination", "/source");

            ASSERT_EQUAL(cmd.argc, 131);
            ASSERT_EQUAL(cmd.argv[63], "t");
            ASSERT_EQUAL(cmd.argv[64], "-e");
            ASSERT_EQUAL(cmd.argv[65], "d");
        ASSERT_EQUAL(cmd.argv[128], "d");
        ASSERT_EQUAL(cmd.argv[129], "/destination");
        ASSERT_EQUAL(cmd.argv[130], "/source");
        ASSERT_MORE(cmd.cap, cmd.argc);
        ASSERT(cmd.argv[cmd.argc] == NULL);

        command_text = command_str(&cmd, &len);
        ASSERT_EQUAL(len, 279);
        free2(command_text, len + 1);
        }

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        {
            enum {
                LONG_COMMAND_ARGUMENT_SIZE = 5000,
            };
            char long_argument[LONG_COMMAND_ARGUMENT_SIZE];
            char *long_argument_string;

            memset64(long_argument, 'x', SIZEOF(long_argument) - 1);
            long_argument[SIZEOF(long_argument) - 1] = '\0';
            long_argument_string = long_argument;
            COMMAND_PUSH(&cmd, long_argument_string);

            command_text = command_str(&cmd, &len);
            ASSERT_EQUAL(len, SIZEOF(long_argument) - 1);
            ASSERT_EQUAL(command_text, long_argument_string);
            free2(command_text, len + 1);
        }

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

#if OS_UNIX
        COMMAND_PUSH(&cmd, "sh", "-c", "exit 7");
        ASSERT(command_run_sync(&cmd, NULL));
        ASSERT_EQUAL(cmd.result.status, 7);
        ASSERT(cmd.result.exited);
        ASSERT_EQUAL(cmd.result.exit_status, 7);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        COMMAND_PUSH(&cmd,
                     "sh",
                     "-c",
                     "printf stdout; printf stderr >&2; exit 7");
        ASSERT(command_run_capture_combined(&cmd));
        ASSERT_EQUAL(cmd.result.output, "stdoutstderr");
        ASSERT_EQUAL(cmd.result.stdout_output, "stdoutstderr");
        ASSERT_EQUAL(cmd.result.stderr_output, "");
        ASSERT_EQUAL(cmd.result.output_len, 12);
        ASSERT_EQUAL(cmd.result.status, 7);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        COMMAND_PUSH(&cmd,
                     "sh",
                     "-c",
                     "printf stdout; printf stderr >&2; exit 6");
        ASSERT(command_run_capture_all(&cmd));
        ASSERT_EQUAL(cmd.result.stdout_output, "stdout");
        ASSERT_EQUAL(cmd.result.stderr_output, "stderr");
        ASSERT_EQUAL(cmd.result.stdout_len, 6);
        ASSERT_EQUAL(cmd.result.stderr_len, 6);
        ASSERT_EQUAL(cmd.result.status, 6);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        COMMAND_PUSH(&cmd, "cat");
        ASSERT_ZERO((command_stdin_buffer_set(&cmd, STRLIT("stdin-buffer"))));
        ASSERT(command_run_capture(&cmd, COMMAND_CAPTURE_STDOUT));
        ASSERT_EQUAL(cmd.result.stdout_output, "stdin-buffer");
        ASSERT_ZERO(cmd.result.status);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);
        ASSERT(cmd.stdin_buffer == NULL);
        ASSERT_NEGATIVE((command_stdin_buffer_set(&cmd, NULL, 0)));

        {
            enum {
                COMMAND_STDIN_TEST_LEN = 100000,
            };
            char *stdin_data;

            stdin_data = malloc2(COMMAND_STDIN_TEST_LEN);
            memset64(stdin_data, 'x', COMMAND_STDIN_TEST_LEN);
            COMMAND_PUSH(&cmd,
                         "sh",
                         "-c",
                         "cat >/dev/null; printf done");
            ASSERT_ZERO((command_stdin_buffer_set(&cmd,
                                                  stdin_data,
                                                  COMMAND_STDIN_TEST_LEN)));
            ASSERT(command_run_capture_all(&cmd));
            ASSERT_EQUAL(cmd.result.stdout_output, "done");
            ASSERT_ZERO(cmd.result.status);
            free2(stdin_data, COMMAND_STDIN_TEST_LEN);
        }

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        {
            enum {
                COMMAND_EPIPE_TEST_LEN = 100000,
            };
            char *stdin_data;

            stdin_data = malloc2(COMMAND_EPIPE_TEST_LEN);
            memset64(stdin_data, 'x', COMMAND_EPIPE_TEST_LEN);
            COMMAND_PUSH(&cmd, "sh", "-c", "exit 3");
            ASSERT_ZERO((command_stdin_buffer_set(&cmd,
                                                  stdin_data,
                                                  COMMAND_EPIPE_TEST_LEN)));
            ASSERT(command_run_capture_all(&cmd));
            ASSERT_EQUAL(cmd.result.status, 3);
            free2(stdin_data, COMMAND_EPIPE_TEST_LEN);
        }

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        {
            char *empty_input = "";

            COMMAND_PUSH(&cmd, "cat");
            ASSERT_ZERO((command_stdin_buffer_set(&cmd, empty_input, 0)));
            ASSERT(command_run_capture(&cmd, COMMAND_CAPTURE_STDOUT));
            ASSERT_EQUAL(cmd.result.stdout_output, "");
            ASSERT_ZERO(cmd.result.status);
        }

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        {
            char expected_cwd[PATH_MAX];
            char test_cwd[PATH_MAX];
            int32 expected_cwd_len;

            test_make_temp_dir(test_cwd, SIZEOF(test_cwd), "command_cwd");
            ASSERT(realpath(test_cwd, expected_cwd) != NULL);
            expected_cwd_len = strlen32(expected_cwd);
            ASSERT_LESS(expected_cwd_len + 1, SIZEOF(expected_cwd));
            expected_cwd[expected_cwd_len] = '\n';
            expected_cwd[expected_cwd_len + 1] = '\0';

            command_cwd_set(&cmd, test_cwd);
            COMMAND_PUSH(&cmd, "pwd", "-P");
            ASSERT(command_run_capture(&cmd, COMMAND_CAPTURE_STDOUT));
            ASSERT_EQUAL(cmd.result.stdout_output, expected_cwd);
            command_cwd_clear(&cmd);
            test_remove_tree(test_cwd);
        }

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        command_env_push(&cmd, "COMMAND_TEST_VALUE=works");
        command_env_printf(&cmd, "COMMAND_TEST_NUMBER=%d", 42);
        COMMAND_PUSH(&cmd,
                     "sh",
                     "-c",
                     "printf %s:%s "
                     "$COMMAND_TEST_VALUE "
                     "$COMMAND_TEST_NUMBER");
        ASSERT(command_run_capture(&cmd, COMMAND_CAPTURE_STDOUT));
        ASSERT_EQUAL(cmd.result.stdout_output, "works:42");
        command_env_clear(&cmd);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);
#endif

#if OS_WINDOWS
        COMMAND_PUSH(&cmd, "cmd", "/C", "exit /B 7");
        ASSERT(command_run_sync(&cmd, NULL));
        ASSERT_EQUAL(cmd.result.status, 7);
        ASSERT(cmd.result.exited);
        ASSERT_EQUAL(cmd.result.exit_status, 7);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        COMMAND_PUSH(&cmd,
                     "cmd",
                     "/C",
                     "echo stdout&echo stderr>&2&exit /B 7");
        ASSERT(command_run_capture_combined(&cmd));
        ASSERT_EQUAL(cmd.result.output, "stdout\r\nstderr\r\n");
        ASSERT_EQUAL(cmd.result.stdout_output, "stdout\r\nstderr\r\n");
        ASSERT_EQUAL(cmd.result.stderr_output, "");
        ASSERT_EQUAL(cmd.result.output_len, 16);
        ASSERT_EQUAL(cmd.result.status, 7);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        COMMAND_PUSH(&cmd,
                     "cmd",
                     "/C",
                     "echo stdout&echo stderr>&2&exit /B 6");
        ASSERT(command_run_capture_all(&cmd));
        ASSERT_EQUAL(cmd.result.stdout_output, "stdout\r\n");
        ASSERT_EQUAL(cmd.result.stderr_output, "stderr\r\n");
        ASSERT_EQUAL(cmd.result.stdout_len, 8);
        ASSERT_EQUAL(cmd.result.stderr_len, 8);
        ASSERT_EQUAL(cmd.result.status, 6);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);
#endif

        {
            char *flags_str;

            flags_str = COMMAND_str(COMMAND_CAPTURE_STDOUT
                                         |COMMAND_CAPTURE_STDERR);
            ASSERT_EQUAL(flags_str,
                         "COMMAND_CAPTURE_STDOUT"
                         "|COMMAND_CAPTURE_STDERR");
            COMMAND_str_free(flags_str);
            ASSERT(COMMAND_parse("CAPTURE_STDOUT|CAPTURE_STDERR")
                   == (COMMAND_CAPTURE_STDOUT |COMMAND_CAPTURE_STDERR));
        }

#if OS_UNIX
        COMMAND_PUSH(&cmd, "sh", "-c", "exit 9");
        ASSERT(command_run_async(&cmd, COMMAND_NEW_PROCESS_GROUP));
        ASSERT_POSITIVE(cmd.result.pid);
        ASSERT_ZERO(command_wait(&cmd));
        ASSERT_EQUAL(cmd.result.status, 9);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        COMMAND_PUSH(&cmd,
                     "sh",
                     "-c",
                     "printf asyncout; printf asyncerr >&2");
        ASSERT(command_run_async(&cmd,
                                 COMMAND_CAPTURE_STDOUT
                                 |COMMAND_CAPTURE_STDERR));
        ASSERT_POSITIVE(cmd.result.pid);
        command_result_read_captured(&cmd);
        ASSERT_ZERO(command_wait(&cmd));
        ASSERT_EQUAL(cmd.result.stdout_output, "asyncout");
        ASSERT_EQUAL(cmd.result.stderr_output, "asyncerr");
        ASSERT_ZERO(cmd.result.status);

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);

        COMMAND_PUSH(&cmd, "sh", "-c", "exit 0");
        ASSERT(command_run(&cmd, COMMAND_DETACHED) == 0);
        ASSERT_ZERO(cmd.result.status);
#endif

        command_reset(&cmd);
        ASSERT_ZERO(cmd.argc);
        command_free(&cmd);
        ASSERT(cmd.argv == NULL);
        ASSERT(cmd.argvs_lens == NULL);
        ASSERT(cmd.env == NULL);
        ASSERT(cmd.env_lens == NULL);
        ASSERT_ZERO(cmd.cap);
        ASSERT_ZERO(cmd.env_cap);
    }

    NCALLS(1);

    time_monotonic_precise(&t1);
    PRINT_TIMINGS(1, t0, t1);
    exit(EXIT_SUCCESS);
}

#endif /* TESTING_command */

#endif /* COMMAND_C */
