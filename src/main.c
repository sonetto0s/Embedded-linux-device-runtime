#include "shell.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int ensure_standard_fd(int target, int flags)
{
    if (fcntl(target, F_GETFD) >= 0)
        return 0;

    if (errno != EBADF)
        return -1;

    int fd = open("/dev/null", flags);
    if (fd < 0)
        return -1;

    if (fd != target) {
        if (dup2(fd, target) < 0) {
            close(fd);
            return -1;
        }

        close(fd);
    }

    return 0;
}

static int ensure_standard_fds(void)
{
    if (ensure_standard_fd(STDIN_FILENO, O_RDONLY) < 0)
        return -1;

    if (ensure_standard_fd(STDOUT_FILENO, O_WRONLY) < 0)
        return -1;

    if (ensure_standard_fd(STDERR_FILENO, O_WRONLY) < 0)
        return -1;

    return 0;
}

int main(void)
{
    if (ensure_standard_fds() < 0)
        return 1;

    ShellContext ctx;

    if (shell_init(&ctx) != SHELL_STATUS_OK)
        return 1;

    ShellStatus run_status = shell_run(&ctx);
    int exit_status = ctx.last_exit_status;

    shell_cleanup(&ctx);

    if (run_status != SHELL_STATUS_OK)
        return 1;

    return exit_status;
}









