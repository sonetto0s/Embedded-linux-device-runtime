#include "cmd_runtime.h"

#include "error.h"
#include "process_manager.h"
#include "runtime_monitor.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void psinfo_usage(void)
{
    printf("usage:\n");
    printf("  psinfo\n");
    printf("  psinfo <pid>\n");
}

static int parse_pid(const char *text, pid_t *pid)
{
    if (!text || !pid)
    {
        return MiniShell_ERR_PARSE;
    }

    errno = 0;

    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (errno != 0 || !end || *end != '\0' || value <= 0 || value > INT_MAX)
    {
        return MiniShell_ERR_PARSE;
    }

    *pid = (pid_t)value;

    return MiniShell_OK;
}

int cmd_monitor(Command *cmd, struct ShellContext *ctx)
{
    (void)ctx;

    if (!cmd || cmd->argc != 1)
    {
        fprintf(stderr, "usage: monitor\n");
        return MiniShell_ERR_PARSE;
    }

    RuntimeMonitor monitor;
    int ret = runtime_monitor_collect(&monitor);

    if (ret != MiniShell_OK)
    {
        fprintf(stderr, "monitor: failed to collect runtime information\n");
        return ret;
    }

    runtime_monitor_print(&monitor);

    return MiniShell_OK;
}

int cmd_psinfo(Command *cmd, struct ShellContext *ctx)
{
    (void)ctx;

    if (!cmd || cmd->argc < 1 || cmd->argc > 2)
    {
        psinfo_usage();
        return MiniShell_ERR_PARSE;
    }

    if (cmd->argc == 2)
    {
        pid_t pid;
        int ret = parse_pid(cmd->argv[1], &pid);

        if (ret != MiniShell_OK)
        {
            fprintf(stderr, "psinfo: invalid pid: %s\n", cmd->argv[1]);
            return ret;
        }

        ProcessInfo info;

        ret = process_manager_get(pid, &info);

        if (ret != MiniShell_OK)
        {
            fprintf(stderr, "psinfo: unable to read process %ld\n", (long)pid);
            return ret;
        }

        process_manager_print_one(&info);

        return MiniShell_OK;
    }

    ProcessInfo *list = NULL;
    size_t count = 0;

    int ret = process_manager_collect(&list, &count);

    if (ret != MiniShell_OK)
    {
        fprintf(stderr, "psinfo: failed to collect process information\n");
        return ret;
    }

    process_manager_print(list, count);
    process_manager_free(list);

    return MiniShell_OK;
}





