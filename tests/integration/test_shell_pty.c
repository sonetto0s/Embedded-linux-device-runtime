#define _XOPEN_SOURCE 600
#include "test_framework.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


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

static int read_until_text(int fd, const char *target, char *output,
                           size_t output_size, int timeout_ms)
{
    if (!target || !output || output_size == 0)
        return -1;

    size_t length = 0;
    output[0] = '\0';

    long long start = monotonic_ms();

    if (start < 0)
        return -1;

    long long deadline = start + timeout_ms;

    while (1) {
        if (strstr(output, target) != NULL)
            return 0;

        long long now = monotonic_ms();

        if (now < 0 || now >= deadline)
            return -1;

        struct pollfd pfd = {
            .fd = fd,
            .events = POLLIN
        };

        int remaining = (int)(deadline - now);

        int ret = poll(&pfd, 1, remaining);

        if (ret < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (ret == 0)
            return -1;

        if (!(pfd.revents & (POLLIN | POLLHUP)))
            continue;

        if (length + 1 >= output_size)
            return -1;

        char ch;

        ssize_t n = read(fd, &ch, 1);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0)
            return -1;

        output[length++] = ch;
        output[length] = '\0';
    }
}

static int wait_for_text(int fd, const char *target, int timeout_ms)
{
    char output[4096];

    return read_until_text(fd,
                           target,
                           output,
                           sizeof(output),
                           timeout_ms);
}

static int wait_for_prompt_with_text(int fd, const char *target, int timeout_ms)
{
    if (!target)
        return -1;

    char output[4096];

    size_t length = 0;
    output[0] = '\0';

    long long start = monotonic_ms();

    if (start < 0)
        return -1;

    long long deadline = start + timeout_ms;

    while (1) {
        char *target_pos = strstr(output, target);

        if (target_pos &&
            strstr(target_pos + strlen(target),
                   ">>MiniShell ") != NULL)
            return 0;

        long long now = monotonic_ms();

        if (now < 0 || now >= deadline)
            return -1;

        struct pollfd pfd = {
            .fd = fd,
            .events = POLLIN
        };

        int remaining = (int)(deadline - now);

        int ret = poll(&pfd, 1, remaining);

        if (ret < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (ret == 0)
            return -1;

        if (!(pfd.revents & (POLLIN | POLLHUP)))
            continue;

        if (length + 1 >= sizeof(output))
            return -1;

        char ch;

        ssize_t n = read(fd, &ch, 1);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0)
            return -1;

        output[length++] = ch;
        output[length] = '\0';
    }
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

static int write_control(int fd, unsigned char control)
{
    while (1) {
        ssize_t n =
            write(fd,
                  &control,
                  1);

        if (n == 1)
            return 0;

        if (n < 0 && errno == EINTR)
            continue;

        return -1;
    }
}

static int wait_for_pid(pid_t pid, int *status, int timeout_ms)
{
    const char *asan_options = getenv("ASAN_OPTIONS");

    if (asan_options && *asan_options && timeout_ms < 15000)
        timeout_ms = 15000;

    long long start = monotonic_ms();

    if (start < 0)
        return -1;

    long long deadline = start + timeout_ms;

    while (1)
    {
        pid_t ret = waitpid(pid, status, WNOHANG);

        if (ret == pid)
            return 0;

        if (ret < 0)
        {
            if (errno == EINTR)
                continue;

            return -1;
        }

        long long now = monotonic_ms();

        if (now < 0 || now >= deadline)
            return -1;

        long long remaining = deadline - now;

        long sleep_ms = remaining > 20 ? 20 : (long)remaining;

        struct timespec delay = {
            .tv_sec = sleep_ms / 1000,
            .tv_nsec = (sleep_ms % 1000) * 1000000L
        };

        while (nanosleep(&delay, &delay) < 0)
        {
            if (errno != EINTR)
                return -1;
        }
    }
}

static void kill_pty_session(pid_t shell_pid, pid_t supervisor_pid)
{
    if (shell_pid > 0)
    {
        kill(-shell_pid, SIGKILL);
        kill(shell_pid, SIGKILL);
    }

    if (supervisor_pid > 0)
    {
        kill(supervisor_pid, SIGKILL);

        while (waitpid(supervisor_pid, NULL, 0) < 0)
        {
            if (errno != EINTR)
                break;
        }
    }
}

static int start_pty_shell(int *master_fd, pid_t *supervisor_pid, pid_t *shell_pid)
{
    int pid_pipe[2];

    *master_fd = -1;
    *supervisor_pid = -1;
    *shell_pid = -1;

    int master = posix_openpt(O_RDWR | O_NOCTTY);

    if (master < 0)
        return -1;

    if (grantpt(master) < 0 || unlockpt(master) < 0)
    {
        close(master);
        return -1;
    }

    char *slave_name = ptsname(master);

    if (slave_name == NULL)
    {
        close(master);
        return -1;
    }

    if (pipe(pid_pipe) < 0)
    {
        close(master);
        return -1;
    }

    pid_t supervisor = fork();

    if (supervisor < 0)
    {
        close(pid_pipe[0]);
        close(pid_pipe[1]);
        close(master);
        return -1;
    }

    if (supervisor == 0)
    {
        close(pid_pipe[0]);
        close(master);

        if (setsid() < 0)
            _exit(127);

        int slave = open(slave_name, O_RDWR);

        if (slave < 0)
            _exit(127);

        if (ioctl(slave, TIOCSCTTY, 0) < 0)
        {
            close(slave);
            _exit(127);
        }

        pid_t shell = fork();

        if (shell < 0)
        {
            close(slave);
            _exit(127);
        }

        if (shell == 0)
        {
            close(pid_pipe[1]);

            if (dup2(slave, STDIN_FILENO) < 0)
                _exit(127);

            if (dup2(slave, STDOUT_FILENO) < 0)
                _exit(127);

            if (dup2(slave, STDERR_FILENO) < 0)
                _exit(127);

            if (slave > STDERR_FILENO)
                close(slave);

           execl(minishell_path(), "minishell", (char *)NULL);
            _exit(127);
        }

        close(slave);

        ssize_t sent;

        do
        {
            sent = write(pid_pipe[1], &shell, sizeof(shell));
        }
        while (sent < 0 && errno == EINTR);

        close(pid_pipe[1]);

        if (sent != (ssize_t)sizeof(shell))
        {
            kill(shell, SIGKILL);
            waitpid(shell, NULL, 0);
            _exit(127);
        }

        int status;

        while (waitpid(shell, &status, 0) < 0)
        {
            if (errno != EINTR)
                _exit(127);
        }

        if (WIFEXITED(status))
            _exit(WEXITSTATUS(status));

        if (WIFSIGNALED(status))
            _exit(128 + WTERMSIG(status));

        _exit(127);
    }

    close(pid_pipe[1]);

    pid_t shell;
    ssize_t received;

    do
    {
        received = read(pid_pipe[0], &shell, sizeof(shell));
    }
    while (received < 0 && errno == EINTR);

    close(pid_pipe[0]);

    if (received != (ssize_t)sizeof(shell))
    {
        kill_pty_session(-1, supervisor);
        close(master);
        return -1;
    }

    *master_fd = master;
    *supervisor_pid = supervisor;
    *shell_pid = shell;

    return 0;
}

void test_shell_pty_start_exit(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret = start_pty_shell(&master_fd, &supervisor_pid, &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
        return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret = wait_for_pid(supervisor_pid, &status, 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
    {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    }
    else
    {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
}

void test_shell_pty_prompt_ctrl_c(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret = start_pty_shell(&master_fd, &supervisor_pid, &shell_pid);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
        return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_control(master_fd, 3);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "echo alive\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
        ret = wait_for_prompt_with_text(master_fd, "alive", 4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
        ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);

    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret = wait_for_pid(supervisor_pid, &status, 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
    {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    }
    else
    {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
}

void test_shell_pty_ctrl_z(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret = start_pty_shell(&master_fd, &supervisor_pid, &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
        return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "sleep 5\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 200000000L
    };

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 26);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_prompt_with_text(master_fd, "now it is stopped", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "fg\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 3);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret = wait_for_pid(supervisor_pid, &status, 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
    {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 130);
    }
    else
    {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
}

void test_shell_pty_bg(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret = start_pty_shell(&master_fd, &supervisor_pid, &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
        return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "sleep 10\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 200000000L
    };

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 26);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_prompt_with_text(master_fd, "now it is stopped", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "bg\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

 ret = wait_for_text(master_fd, ">>MiniShell ", 5000);
TEST_ASSERT_EQ(ret, 0);

if (ret != 0)
{
    kill_pty_session(shell_pid, supervisor_pid);
    close(master_fd);
    return;
}

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_prompt_with_text(master_fd, "now it is running", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret = wait_for_pid(supervisor_pid, &status, 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
    {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    }
    else
    {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
}

void test_shell_pty_fg(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret = start_pty_shell(&master_fd, &supervisor_pid, &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
        return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "sleep 5 &\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_prompt_with_text(master_fd, "now it is running", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "fg\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 300);
    TEST_ASSERT_EQ(ret, -1);

    if (ret == 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 200000000L
    };

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 3);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    char output[4096];

    ret = read_until_text(master_fd, ">>MiniShell ", output, sizeof(output), 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    TEST_ASSERT(strstr(output, "sleep") == NULL);
    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }
    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;
    ret = wait_for_pid(supervisor_pid, &status, 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
    {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    }
    else
    {
        kill_pty_session(shell_pid, supervisor_pid);
    }
    close(master_fd);
}

void test_shell_pty_ctrl_c(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret = start_pty_shell(&master_fd, &supervisor_pid, &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
        return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "sleep 5\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 200000000L
    };

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 3);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret = wait_for_pid(supervisor_pid, &status, 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
    {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 130);
    }
    else
    {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
}
void test_shell_pty_pipeline_ctrl_z_fg(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret = start_pty_shell(&master_fd, &supervisor_pid, &shell_pid);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
        return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "sleep 10 | sleep 10\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 200000000L
    };

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 26);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
        ret = wait_for_prompt_with_text(master_fd, "now it is stopped", 4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "fg\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 300);
    TEST_ASSERT_EQ(ret, -1);

    if (ret == 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 3);
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
        ret = wait_for_text(master_fd, ">>MiniShell ", 4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0)
    {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    char output[4096];

    if (ret == 0)
        ret = read_until_text(master_fd, ">>MiniShell ", output, sizeof(output), 4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
    {
        TEST_ASSERT(strstr(output, "now it is running") == NULL &&
                    strstr(output, "now it is stopped") == NULL);
    }

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
        ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);

    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret = wait_for_pid(supervisor_pid, &status, 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
    {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    }
    else
    {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
}
void test_shell_pty_background_tty_stop(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret = start_pty_shell(&master_fd, &supervisor_pid, &shell_pid);
    TEST_ASSERT_EQ(ret, 0);
    if (ret != 0) return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "cat &\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = wait_for_text(master_fd, "stopped by 21", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        kill_pty_session(shell_pid, supervisor_pid);
        close(master_fd);
        return;
    }

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        ret = wait_for_prompt_with_text(master_fd, "now it is stopped", 4000);
    }

    TEST_ASSERT_EQ(ret, 0);

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    }

    TEST_ASSERT_EQ(ret, 0);

    int status = 0;
    ret = wait_for_pid(supervisor_pid, &status, 4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    } else {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
}



static int pty_test_path(char *buffer, size_t size, const char *name)
{
    const char *dir = getenv("MINISHELL_TEST_DIR");

    if (!dir || !*dir)
        dir = "/tmp";

    int written =
        snprintf(buffer,
                 size,
                 "%s/%s",
                 dir,
                 name);

    return written >= 0 &&
           (size_t)written < size
               ? 0
               : -1;
}


void test_shell_pty_fg_redirect_stdin(void)
{
    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret =
        start_pty_shell(&master_fd,
                        &supervisor_pid,
                        &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) return;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = write_all(master_fd, "sleep 10 &\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = write_all(master_fd, "fg < /dev/null\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 300);
    TEST_ASSERT_EQ(ret, -1);

    if (ret == 0) goto fail;

    ret = write_control(master_fd, 3);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret =
        wait_for_pid(supervisor_pid,
                     &status,
                     4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 130);
    } else {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);

    return;

fail:
    kill_pty_session(shell_pid, supervisor_pid);
    close(master_fd);
}



void test_shell_pty_fifo_ctrl_c(void)
{
    char fifo_path[512];

    int path_ret =
        pty_test_path(fifo_path,
                      sizeof(fifo_path),
                      "minishell_fifo_ctrl_c");

    TEST_ASSERT_EQ(path_ret, 0);

    if (path_ret != 0) return;

    unlink(fifo_path);

    int fifo_ret =
        mkfifo(fifo_path, 0600);

    TEST_ASSERT_EQ(fifo_ret, 0);

    if (fifo_ret != 0) return;

    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret =
        start_pty_shell(&master_fd,
                        &supervisor_pid,
                        &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        unlink(fifo_path);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    char command[640];

    int written =
        snprintf(command,
                 sizeof(command),
                 "cat < %s\n",
                 fifo_path);

    TEST_ASSERT(written > 0 &&
                (size_t)written < sizeof(command));

    if (written <= 0 ||
        (size_t)written >= sizeof(command))
        goto fail;

    ret = write_all(master_fd, command);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 150000000L
    };

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 3);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret =
        wait_for_pid(supervisor_pid,
                     &status,
                     4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 130);
    } else {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
    unlink(fifo_path);

    return;

fail:
    kill_pty_session(shell_pid, supervisor_pid);
    close(master_fd);
    unlink(fifo_path);
}




void test_shell_pty_fifo_ctrl_z(void)
{
    char fifo_path[512];

    int path_ret =
        pty_test_path(fifo_path,
                      sizeof(fifo_path),
                      "minishell_fifo_ctrl_z");

    TEST_ASSERT_EQ(path_ret, 0);

    if (path_ret != 0) return;

    unlink(fifo_path);

    int fifo_ret =
        mkfifo(fifo_path, 0600);

    TEST_ASSERT_EQ(fifo_ret, 0);

    if (fifo_ret != 0) return;

    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret =
        start_pty_shell(&master_fd,
                        &supervisor_pid,
                        &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        unlink(fifo_path);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    char command[640];

    int written =
        snprintf(command,
                 sizeof(command),
                 "cat < %s\n",
                 fifo_path);

    TEST_ASSERT(written > 0 &&
                (size_t)written < sizeof(command));

    if (written <= 0 ||
        (size_t)written >= sizeof(command))
        goto fail;

    ret = write_all(master_fd, command);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 150000000L
    };

    nanosleep(&delay, NULL);

    ret = write_control(master_fd, 26);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = write_all(master_fd, "jobs\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0)
        ret =
            wait_for_prompt_with_text(
                master_fd,
                "now it is stopped",
                4000
            );

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = write_all(master_fd, "fg\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 300);
    TEST_ASSERT_EQ(ret, -1);

    if (ret == 0) goto fail;

    ret = write_control(master_fd, 3);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret =
        wait_for_pid(supervisor_pid,
                     &status,
                     4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 130);
    } else {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
    unlink(fifo_path);

    return;

fail:
    kill_pty_session(shell_pid, supervisor_pid);
    close(master_fd);
    unlink(fifo_path);
}



void test_shell_pty_termios_stop_fg(void)
{
    char script_path[512];

    int path_ret =
        pty_test_path(script_path,
                      sizeof(script_path),
                      "minishell_termios_stop.sh");

    TEST_ASSERT_EQ(path_ret, 0);

    if (path_ret != 0) return;

    FILE *fp =
        fopen(script_path, "w");

    TEST_ASSERT_NOT_NULL(fp);

    if (!fp) return;

    fprintf(fp, "#!/bin/sh\n");
    fprintf(fp, "stty -echo -icanon\n");
    fprintf(fp, "before=$(stty -g)\n");
    fprintf(fp, "kill -STOP $$\n");
    fprintf(fp, "after=$(stty -g)\n");
    fprintf(fp, "if [ \"$before\" != \"$after\" ]; then stty echo icanon; exit 42; fi\n");
    fprintf(fp, "stty echo icanon\n");
    fprintf(fp, "exit 0\n");

    fclose(fp);

    int chmod_ret =
        chmod(script_path, 0700);

    TEST_ASSERT_EQ(chmod_ret, 0);

    if (chmod_ret != 0) {
        unlink(script_path);
        return;
    }

    int master_fd;
    pid_t supervisor_pid;
    pid_t shell_pid;

    int ret =
        start_pty_shell(&master_fd,
                        &supervisor_pid,
                        &shell_pid);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        unlink(script_path);
        return;
    }

    ret = wait_for_text(master_fd, ">>MiniShell ", 4000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    char command[640];

    int written =
        snprintf(command,
                 sizeof(command),
                 "%s\n",
                 script_path);

    TEST_ASSERT(written > 0 &&
                (size_t)written < sizeof(command));

    if (written <= 0 ||
        (size_t)written >= sizeof(command))
        goto fail;

    ret = write_all(master_fd, command);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 5000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    struct termios shell_modes;

    ret =
        tcgetattr(master_fd,
                  &shell_modes);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        TEST_ASSERT((shell_modes.c_lflag & ECHO) != 0);
        TEST_ASSERT((shell_modes.c_lflag & ICANON) != 0);
    }

    ret = write_all(master_fd, "fg\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell ", 5000);
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret =
        tcgetattr(master_fd,
                  &shell_modes);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        TEST_ASSERT((shell_modes.c_lflag & ECHO) != 0);
        TEST_ASSERT((shell_modes.c_lflag & ICANON) != 0);
    }

    ret = write_all(master_fd, "exit\n");
    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) goto fail;

    ret = wait_for_text(master_fd, ">>MiniShell 已退出", 4000);
    TEST_ASSERT_EQ(ret, 0);

    int status = 0;

    ret =
        wait_for_pid(supervisor_pid,
                     &status,
                     4000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    } else {
        kill_pty_session(shell_pid, supervisor_pid);
    }

    close(master_fd);
    unlink(script_path);

    return;

fail:
    kill_pty_session(shell_pid, supervisor_pid);
    close(master_fd);
    unlink(script_path);
}



void test_shell_pty_background_start_stops(void)
{
    int master =
        posix_openpt(O_RDWR | O_NOCTTY);

    TEST_ASSERT(master >= 0);

    if (master < 0) return;

    int ret = grantpt(master);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        close(master);
        return;
    }

    ret = unlockpt(master);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        close(master);
        return;
    }

    char *slave_name =
        ptsname(master);

    TEST_ASSERT_NOT_NULL(slave_name);

    if (!slave_name) {
        close(master);
        return;
    }

    int result_pipe[2];

    ret = pipe(result_pipe);

    TEST_ASSERT_EQ(ret, 0);

    if (ret != 0) {
        close(master);
        return;
    }

    pid_t supervisor = fork();

    TEST_ASSERT(supervisor >= 0);

    if (supervisor < 0) {
        close(result_pipe[0]);
        close(result_pipe[1]);
        close(master);
        return;
    }

    if (supervisor == 0) {
        close(result_pipe[0]);
        close(master);

        if (setsid() < 0)
            _exit(2);

        int slave =
            open(slave_name, O_RDWR);

        if (slave < 0)
            _exit(3);

        if (ioctl(slave, TIOCSCTTY, 0) < 0)
            _exit(4);

        if (tcsetpgrp(slave, getpgrp()) < 0)
            _exit(5);

        pid_t shell = fork();

        if (shell < 0)
            _exit(6);

        if (shell == 0) {
            close(result_pipe[1]);

            if (setpgid(0, 0) < 0)
                _exit(7);

            if (dup2(slave, STDIN_FILENO) < 0 ||
                dup2(slave, STDOUT_FILENO) < 0 ||
                dup2(slave, STDERR_FILENO) < 0)
                _exit(8);

            if (slave > STDERR_FILENO)
                close(slave);

            execl(minishell_path(),
                  "minishell",
                  (char *)NULL);

            _exit(9);
        }

        close(slave);

        long long start =
            monotonic_ms();

        int stopped = 0;
        int status = 0;

        while (start >= 0 &&
               monotonic_ms() - start < 2000) {
            pid_t wait_ret =
                waitpid(shell,
                        &status,
                        WNOHANG | WUNTRACED);

            if (wait_ret == shell) {
                if (WIFSTOPPED(status) &&
                    WSTOPSIG(status) == SIGTTIN)
                    stopped = 1;

                break;
            }

            if (wait_ret < 0 &&
                errno != EINTR)
                break;

            struct timespec delay = {
                .tv_sec = 0,
                .tv_nsec = 10000000L
            };

            nanosleep(&delay, NULL);
        }

        char result =
            stopped ? 1 : 0;

        ssize_t sent;

        do {
            sent =
                write(result_pipe[1],
                      &result,
                      1);
        } while (sent < 0 &&
                 errno == EINTR);

        close(result_pipe[1]);

        kill(shell, SIGKILL);

        while (waitpid(shell, NULL, 0) < 0) {
            if (errno != EINTR)
                break;
        }

        _exit(stopped && sent == 1
                  ? 0
                  : 1);
    }

    close(result_pipe[1]);

    char result = 0;

    ssize_t received;

    do {
        received =
            read(result_pipe[0],
                 &result,
                 1);
    } while (received < 0 &&
             errno == EINTR);

    close(result_pipe[0]);

    TEST_ASSERT_EQ(received, 1);
    TEST_ASSERT_EQ(result, 1);

    int status = 0;

    ret =
        wait_for_pid(supervisor,
                     &status,
                     5000);

    TEST_ASSERT_EQ(ret, 0);

    if (ret == 0) {
        TEST_ASSERT(WIFEXITED(status));
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    } else {
        kill(supervisor, SIGKILL);
        waitpid(supervisor, NULL, 0);
    }

    close(master);
}
