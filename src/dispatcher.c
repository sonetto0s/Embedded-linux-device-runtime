#include "dispatcher.h"
#include "builtin.h"
#include "builtin_table.h"
#include "error.h"
#include "executor.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int dup_cloexec(int fd)
{
    return fcntl(fd, F_DUPFD_CLOEXEC, 3);
}

static int dup2_retry(int oldfd, int newfd)
{
    while (dup2(oldfd, newfd) < 0)
    {
        if (errno == EINTR)
            continue;

        return -1;
    }

    return 0;
}

static int builtin_apply_redirect(Command *cmd, int *saved_out, int *saved_in)
{
    *saved_out = -1;
    *saved_in = -1;

    if (cmd->redirect.output_file)
     {
        errno = 0;

        if (fflush(stdout) != 0 || ferror(stdout))
        {
            int saved_errno = errno;
            clearerr(stdout);

            if (saved_errno != 0)
                log_error("flush stdout before redirect failed: %s",
                          strerror(saved_errno));
            else
                log_error("flush stdout before redirect failed");

            return MiniShell_ERR_UNKNOWN;
        }

        *saved_out = dup_cloexec(STDOUT_FILENO);

        if (*saved_out < 0)
        {
            log_error("backup stdout failed: %s", strerror(errno));
            return MiniShell_ERR_DUP2;
        }

        int flags = cmd->redirect.append
                        ? O_WRONLY | O_CREAT | O_APPEND
                        : O_WRONLY | O_CREAT | O_TRUNC;

        int fd = open(cmd->redirect.output_file, flags, 0644);

        if (fd < 0)
        {
            log_error("open output '%s' failed: %s",
                      cmd->redirect.output_file, strerror(errno));
            return MiniShell_ERR_OPEN;
        }

        if (fd != STDOUT_FILENO)
        {
            if (dup2_retry(fd, STDOUT_FILENO) < 0)
            {
                int saved_errno = errno;
                close(fd);
                log_error("redirect stdout failed: %s", strerror(saved_errno));
                return MiniShell_ERR_DUP2;
            }

            close(fd);
        }

        clearerr(stdout);
    }


    if (cmd->redirect.input_file)
    {
        *saved_in = dup_cloexec(STDIN_FILENO);

        if (*saved_in < 0)
        {
            log_error("backup stdin failed: %s", strerror(errno));
            return MiniShell_ERR_DUP2;
        }

        int fd = open(cmd->redirect.input_file, O_RDONLY);

        if (fd < 0)
        {
            log_error("open input '%s' failed: %s",
                      cmd->redirect.input_file, strerror(errno));
            return MiniShell_ERR_OPEN;
        }

        if (fd != STDIN_FILENO)
        {
            if (dup2_retry(fd, STDIN_FILENO) < 0)
            {
                int saved_errno = errno;
                close(fd);
                log_error("redirect stdin failed: %s", strerror(saved_errno));
                return MiniShell_ERR_DUP2;
            }

            close(fd);
        }

        clearerr(stdin);
    }

    return MiniShell_OK;
}

static int builtin_restore_redirect(int saved_out,
                                    int saved_in)
{
    int result = MiniShell_OK;

    if (saved_out >= 0)
    {
        if (dup2_retry(saved_out, STDOUT_FILENO) < 0)
        {
            log_error("restore stdout failed: %s",
                      strerror(errno));

            result = MiniShell_ERR_DUP2;
        }

        close(saved_out);
        clearerr(stdout);
    }

    if (saved_in >= 0)
    {
        if (dup2_retry(saved_in, STDIN_FILENO) < 0)
        {
            log_error("restore stdin failed: %s",
                      strerror(errno));

            result = MiniShell_ERR_DUP2;
        }

        close(saved_in);
        clearerr(stdin);
    }

    return result;
}

static int command_chain_has_builtin(const Command *cmd)
{
    for (const Command *current = cmd;current;current = current->next)
    {
        if (current->argv[0] &&builtin_lookup(current->argv[0]))
            return 1;
    }
    return 0;
}

int dispatcher_command(Command *cmd, ShellContext *ctx)
{
    if (!cmd || !ctx || !cmd->argv[0])
        return MiniShell_ERR_UNKNOWN;

    if (cmd->next &&command_chain_has_builtin(cmd))
    {
        log_error("builtin commands in pipelines are not supported");
        return MiniShell_ERR_PARSE;
    }

    BuiltinEntry *entry =builtin_lookup(cmd->argv[0]);

    if (entry && cmd->background)
    {
        log_error("background builtin commands are not supported");
        return MiniShell_ERR_PARSE;
    }

    if (!entry)
        return execute_command(cmd, ctx);

    int saved_out;
    int saved_in;

    int ret = builtin_apply_redirect(cmd, &saved_out, &saved_in);

    if (ret != MiniShell_OK)
    {
        int restore = builtin_restore_redirect(saved_out, saved_in);

        if (restore != MiniShell_OK)
            ctx->running = 0;

        return ret;
    }

    ret = entry->handler(cmd, ctx);
    errno = 0;
    if (fflush(stdout) != 0 || ferror(stdout))
    {
        int saved_errno = errno;
        if (saved_errno != 0)
            log_error("builtin output failed: %s",
                      strerror(saved_errno));
        else
            log_error("builtin output failed");

        clearerr(stdout);

        if (ret == MiniShell_OK)
            ret = MiniShell_ERR_UNKNOWN;
    }

    int restore = builtin_restore_redirect(saved_out, saved_in);

    if (restore != MiniShell_OK)
    {
        ctx->running = 0;
        return restore;
    }

    return ret;
}




