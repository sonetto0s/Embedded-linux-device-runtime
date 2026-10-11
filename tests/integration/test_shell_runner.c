#include "test_framework.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define RUNNER_TIMEOUT_MS 10000

static const char *minishell_path(void)
{
    const char *path = getenv("MINISHELL_TEST_BIN");

    if (path && *path) return path;

    return "./build/default/minishell";
}

static long long monotonic_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return -1;

    return (long long)ts.tv_sec * 1000LL +
           (long long)ts.tv_nsec / 1000000LL;
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);

    if (flags < 0)
        return -1;

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int append_data(char **buffer, size_t *length, size_t *capacity,
                       const char *data, size_t count)
{
    if (!buffer || !length || !capacity || !data)
        return -1;

    if (*length + count + 1 > *capacity) {
        size_t new_capacity = *capacity ? *capacity : 4096;

        while (*length + count + 1 > new_capacity)
            new_capacity *= 2;

        char *new_buffer = realloc(*buffer, new_capacity);

        if (!new_buffer)
            return -1;

        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *length, data, count);

    *length += count;
    (*buffer)[*length] = '\0';

    return 0;
}

static int open_high_fds(void)
{
    struct rlimit limit;
    rlim_t required = (rlim_t)FD_SETSIZE + 16;

    if (getrlimit(RLIMIT_NOFILE, &limit) < 0)
        return -1;

    if (limit.rlim_cur < required) {
        if (limit.rlim_max != RLIM_INFINITY && limit.rlim_max < required) {
            errno = EMFILE;
            return -1;
        }

        /* This helper runs only in the test child, after fork. */
        limit.rlim_cur = required;

        if (setrlimit(RLIMIT_NOFILE, &limit) < 0)
            return -1;
    }

    int last_fd = -1;

    while (last_fd < FD_SETSIZE + 8) {
        last_fd = open("/dev/null", O_RDONLY);

        if (last_fd < 0)
            return -1;
    }

    return 0;
}

static int spawn_shell(int *input_fd, int *output_fd, pid_t *pid, int high_fds)
{
    int input_pipe[2];
    int output_pipe[2];

    if (!input_fd || !output_fd || !pid)
        return -1;

    *input_fd = -1;
    *output_fd = -1;
    *pid = -1;

    if (pipe(input_pipe) < 0)
        return -1;

    if (pipe(output_pipe) < 0) {
        close(input_pipe[0]);
        close(input_pipe[1]);
        return -1;
    }

    pid_t child = fork();

    if (child < 0) {
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        return -1;
    }

    if (child == 0) {
        if (dup2(input_pipe[0], STDIN_FILENO) < 0)
            _exit(127);

        if (dup2(output_pipe[1], STDOUT_FILENO) < 0)
            _exit(127);

        if (dup2(output_pipe[1], STDERR_FILENO) < 0)
            _exit(127);

        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);

        if (high_fds && open_high_fds() < 0)
            _exit(126);

        execl(minishell_path(), "minishell", (char *)NULL);

        _exit(127);
    }

    close(input_pipe[0]);
    close(output_pipe[1]);

    *input_fd = input_pipe[1];
    *output_fd = output_pipe[0];
    *pid = child;

    return 0;
}

static void kill_runner_child(pid_t pid)
{
    if (pid <= 0) return;

    kill(pid, SIGKILL);

    while (waitpid(pid, NULL, 0) < 0) {
        if (errno != EINTR)
            break;
    }
}

static int collect_script(pid_t pid, int input_fd, int output_fd,
                          const char *script, char **output, int *status,
                          int timeout_ms)
{
    size_t script_length = script ? strlen(script) : 0;
    size_t written = 0;
    size_t length = 0;
    size_t capacity = 4096;

    char *buffer = malloc(capacity);

    if (!buffer) {
        close(input_fd);
        close(output_fd);
        kill_runner_child(pid);
        return -1;
    }

    buffer[0] = '\0';

    if (set_nonblocking(input_fd) < 0 ||
        set_nonblocking(output_fd) < 0) {
        free(buffer);
        close(input_fd);
        close(output_fd);
        kill_runner_child(pid);
        return -1;
    }

    int input_open = 1;
    int output_open = 1;
    int child_done = 0;
    int child_status = 0;

    long long start = monotonic_ms();

    if (start < 0) {
        free(buffer);
        close(input_fd);
        close(output_fd);
        kill_runner_child(pid);
        return -1;
    }

    long long deadline = start + timeout_ms;

    while (output_open || !child_done) {
        if (input_open && written >= script_length) {
            close(input_fd);
            input_fd = -1;
            input_open = 0;
        }

        struct pollfd fds[2];

        nfds_t nfds = 0;

        int input_index = -1;
        int output_index = -1;

        if (input_open) {
            input_index = (int)nfds;

            fds[nfds].fd = input_fd;
            fds[nfds].events = POLLOUT;
            fds[nfds].revents = 0;

            nfds++;
        }

        if (output_open) {
            output_index = (int)nfds;

            fds[nfds].fd = output_fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;

            nfds++;
        }

        long long now = monotonic_ms();

        if (now < 0 || now >= deadline)
            goto fail;

        int wait_ms = (int)(deadline - now);

        if (wait_ms > 100)
            wait_ms = 100;

        int poll_ret = poll(fds, nfds, wait_ms);

        if (poll_ret < 0) {
            if (errno != EINTR)
                goto fail;
        } else if (poll_ret > 0) {
            if (input_index >= 0 &&
                (fds[input_index].revents &
                 (POLLOUT | POLLERR | POLLHUP))) {
                if (fds[input_index].revents & POLLOUT) {
                    ssize_t n =
                        write(input_fd,
                              script + written,
                              script_length - written);

                    if (n > 0) {
                        written += (size_t)n;
                    } else if (n < 0 &&
                               errno != EINTR &&
                               errno != EAGAIN &&
                               errno != EWOULDBLOCK) {
                        goto fail;
                    }
                } else {
                    goto fail;
                }
            }

            if (output_index >= 0 &&
                (fds[output_index].revents &
                 (POLLIN | POLLHUP | POLLERR))) {
                while (1) {
                    char chunk[1024];

                    ssize_t n =
                        read(output_fd,
                             chunk,
                             sizeof(chunk));

                    if (n > 0) {
                        if (append_data(&buffer,
                                        &length,
                                        &capacity,
                                        chunk,
                                        (size_t)n) < 0)
                            goto fail;

                        continue;
                    }

                    if (n == 0) {
                        close(output_fd);
                        output_fd = -1;
                        output_open = 0;
                        break;
                    }

                    if (errno == EINTR)
                        continue;

                    if (errno == EAGAIN ||
                        errno == EWOULDBLOCK)
                        break;

                    goto fail;
                }
            }
        }

        if (!child_done) {
            pid_t ret =
                waitpid(pid,
                        &child_status,
                        WNOHANG);

            if (ret == pid) {
                child_done = 1;
            } else if (ret < 0 && errno != EINTR) {
                goto fail;
            }
        }
    }

    if (!child_done) {
        while (waitpid(pid, &child_status, 0) < 0) {
            if (errno != EINTR)
                goto fail;
        }
    }

    if (input_open)
        close(input_fd);

    if (output_open)
        close(output_fd);

    *output = buffer;

    if (status)
        *status = child_status;

    return 0;

fail:
    if (input_open)
        close(input_fd);

    if (output_open)
        close(output_fd);

    free(buffer);

    kill_runner_child(pid);

    return -1;
}

int shell_run_script(const char *script, char **output, int *status)
{
    if (!script || !output)
        return -1;

    *output = NULL;

    int input_fd;
    int output_fd;
    pid_t pid;

    if (spawn_shell(&input_fd,
                    &output_fd,
                    &pid,
                    0) < 0)
        return -1;

    return collect_script(pid,
                          input_fd,
                          output_fd,
                          script,
                          output,
                          status,
                          RUNNER_TIMEOUT_MS);
}

static int write_all(int fd, const char *text)
{
    size_t length = strlen(text);
    size_t written = 0;

    while (written < length) {
        ssize_t n =
            write(fd,
                  text + written,
                  length - written);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        written += (size_t)n;
    }

    return 0;
}

static int read_for_window(int fd, char **buffer, size_t *length,
                           size_t *capacity, int timeout_ms)
{
    long long start = monotonic_ms();

    if (start < 0)
        return -1;

    long long deadline = start + timeout_ms;

    if (set_nonblocking(fd) < 0)
        return -1;

    while (1) {
        long long now = monotonic_ms();

        if (now < 0)
            return -1;

        if (now >= deadline)
            return 0;

        struct pollfd pfd = {
            .fd = fd,
            .events = POLLIN
        };

        int wait_ms = (int)(deadline - now);

        int ret = poll(&pfd, 1, wait_ms);

        if (ret < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (ret == 0)
            return 0;

        if (pfd.revents & (POLLIN | POLLHUP)) {
            while (1) {
                char chunk[512];

                ssize_t n =
                    read(fd,
                         chunk,
                         sizeof(chunk));

                if (n > 0) {
                    if (append_data(buffer,
                                    length,
                                    capacity,
                                    chunk,
                                    (size_t)n) < 0)
                        return -1;

                    continue;
                }

                if (n == 0)
                    return 0;

                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN ||
                    errno == EWOULDBLOCK)
                    break;

                return -1;
            }
        }
    }
}

int shell_run_fragmented_input_test(char **output, int *status)
{
    if (!output)
        return -1;

    *output = NULL;

    int input_fd;
    int output_fd;
    pid_t pid;

    if (spawn_shell(&input_fd,
                    &output_fd,
                    &pid,
                    0) < 0)
        return -1;

    size_t capacity = 4096;
    size_t length = 0;

    char *buffer = malloc(capacity);

    if (!buffer) {
        close(input_fd);
        close(output_fd);
        kill_runner_child(pid);
        return -1;
    }

    buffer[0] = '\0';

    if (write_all(input_fd, "sleep 0.2 &\n") < 0 ||
        write_all(input_fd, "echo PART") < 0)
        goto fail;

    if (read_for_window(output_fd,
                        &buffer,
                        &length,
                        &capacity,
                        600) < 0)
        goto fail;

    if (strstr(buffer, "PART\n") != NULL)
        goto fail;

    if (write_all(input_fd, "IAL\nexit\n") < 0)
        goto fail;

    close(input_fd);
    input_fd = -1;

    if (set_nonblocking(output_fd) < 0)
        goto fail;

    long long start = monotonic_ms();

    if (start < 0)
        goto fail;

    long long deadline =
        start + RUNNER_TIMEOUT_MS;

    int child_done = 0;
    int child_status = 0;

    while (!child_done) {
        long long now = monotonic_ms();

        if (now < 0 || now >= deadline)
            goto fail;

        struct pollfd pfd = {
            .fd = output_fd,
            .events = POLLIN
        };

        int wait_ms = (int)(deadline - now);

        if (wait_ms > 100)
            wait_ms = 100;

        int ret = poll(&pfd, 1, wait_ms);

        if (ret < 0 && errno != EINTR)
            goto fail;

        if (ret > 0 &&
            (pfd.revents & (POLLIN | POLLHUP))) {
            while (1) {
                char chunk[512];

                ssize_t n =
                    read(output_fd,
                         chunk,
                         sizeof(chunk));

                if (n > 0) {
                    if (append_data(&buffer,
                                    &length,
                                    &capacity,
                                    chunk,
                                    (size_t)n) < 0)
                        goto fail;

                    continue;
                }

                if (n == 0)
                    break;

                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN ||
                    errno == EWOULDBLOCK)
                    break;

                goto fail;
            }
        }

        pid_t wait_ret =
            waitpid(pid,
                    &child_status,
                    WNOHANG);

        if (wait_ret == pid)
            child_done = 1;
        else if (wait_ret < 0 && errno != EINTR)
            goto fail;
    }

    while (1) {
        char chunk[512];

        ssize_t n =
            read(output_fd,
                 chunk,
                 sizeof(chunk));

        if (n > 0) {
            if (append_data(&buffer,
                            &length,
                            &capacity,
                            chunk,
                            (size_t)n) < 0)
                goto fail;

            continue;
        }

        if (n < 0 && errno == EINTR)
            continue;

        break;
    }

    close(output_fd);
    output_fd = -1;

    *output = buffer;

    if (status)
        *status = child_status;

    return 0;

fail:
    if (input_fd >= 0)
        close(input_fd);

    if (output_fd >= 0)
        close(output_fd);

    free(buffer);

    kill_runner_child(pid);

    return -1;
}

int shell_run_high_fd_test(char **output, int *status)
{
    if (!output)
        return -1;

    *output = NULL;

    int input_fd;
    int output_fd;
    pid_t pid;

    if (spawn_shell(&input_fd,
                    &output_fd,
                    &pid,
                    1) < 0)
        return -1;

    return collect_script(pid,
                          input_fd,
                          output_fd,
                          "",
                          output,
                          status,
                          RUNNER_TIMEOUT_MS);
}
