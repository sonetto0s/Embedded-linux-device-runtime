#include "cmd_runtime.h"

#include "error.h"
#include "process_monitor.h"
#include "runtime_snapshot.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void print_uptime(double uptime)
{
    unsigned long long total_seconds = (unsigned long long)uptime;
    unsigned long long days = total_seconds / 86400ULL;
    unsigned long long hours = (total_seconds % 86400ULL) / 3600ULL;
    unsigned long long minutes = (total_seconds % 3600ULL) / 60ULL;
    unsigned long long seconds = total_seconds % 60ULL;

    if (days > 0)
    {
        printf("%llud %02llu:%02llu:%02llu", days, hours, minutes, seconds);
        return;
    }

    printf("%02llu:%02llu:%02llu", hours, minutes, seconds);
}

static void print_runtime_snapshot(const RuntimeSnapshot *snapshot)
{
    if (!snapshot)
    {
        return;
    }

    const RuntimeMonitor *runtime = &snapshot->runtime;
    const ThermalRuntimeInfo *thermal = runtime_snapshot_hottest_thermal(snapshot);
    const NetworkRuntimeInfo *network = runtime_snapshot_primary_network(snapshot);

    printf("\n========== Runtime Monitor ==========\n");
    printf("CPU Usage       : %.1f %%\n", runtime->cpu_usage);
    printf("Memory Usage    : %.1f %%\n", runtime->memory_usage);
    printf("Load Average    : %.2f %.2f %.2f\n",
           runtime->load_average[0], runtime->load_average[1], runtime->load_average[2]);

    if (thermal)
    {
        printf("Temperature     : %.1f C", thermal->temperature);

        if (thermal->type[0])
        {
            printf(" (%s)", thermal->type);
        }

        printf("\n");
    }
    else
    {
        printf("Temperature     : N/A\n");
    }

    printf("Process Count   : %u\n", runtime->process_count);

    if (network)
    {
        printf("Network         : %s (%s)\n",
               network->name, network->state[0] ? network->state : "N/A");

        if (network->has_rx)
        {
            printf("RX Bytes        : %llu\n", network->rx_bytes);
        }
        else
        {
            printf("RX Bytes        : N/A\n");
        }

        if (network->has_tx)
        {
            printf("TX Bytes        : %llu\n", network->tx_bytes);
        }
        else
        {
            printf("TX Bytes        : N/A\n");
        }
    }
    else
    {
        printf("Network         : N/A\n");
        printf("RX Bytes        : N/A\n");
        printf("TX Bytes        : N/A\n");
    }

    printf("Uptime          : ");
    print_uptime(runtime->uptime);
    printf("\n");
    printf("=====================================\n\n");
}

static void print_process_info(const ProcessInfo *info)
{
    if (!info)
    {
        return;
    }

    printf("\n========== Process Information ==========\n\n");
    printf("PID             : %ld\n", (long)info->pid);
    printf("Name            : %s\n", info->name);
    printf("State           : %c\n", info->state);
    printf("RSS             : %lu KB\n", info->rss_kb);
    printf("Threads         : %u\n", info->threads);
    printf("\n=========================================\n\n");
}

static void print_process_list(const ProcessInfo *list, size_t count)
{
    if (!list || count == 0)
    {
        printf("no processes found\n");
        return;
    }

    printf("\n========== Process Information ==========\n\n");
    printf("%-8s %-24s %-7s %-12s %-8s\n",
           "PID", "NAME", "STATE", "RSS(KB)", "THREADS");
    printf("----------------------------------------------------------------\n");

    for (size_t i = 0; i < count; i++)
    {
        printf("%-8ld %-24s %-7c %-12lu %-8u\n",
               (long)list[i].pid, list[i].name, list[i].state,
               list[i].rss_kb, list[i].threads);
    }

    printf("\nTotal: %zu processes\n", count);
    printf("=========================================\n\n");
}

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

    RuntimeSnapshot snapshot;
    int ret = runtime_snapshot_collect(&snapshot);

    if (ret != MiniShell_OK)
    {
        fprintf(stderr, "monitor: failed to collect runtime information\n");
        return ret;
    }

    print_runtime_snapshot(&snapshot);

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

        ret = process_monitor_get(pid, &info);

        if (ret != MiniShell_OK)
        {
            fprintf(stderr, "psinfo: unable to read process %ld\n", (long)pid);
            return ret;
        }

        print_process_info(&info);

        return MiniShell_OK;
    }

    ProcessInfo *list = NULL;
    size_t count = 0;

    int ret = process_monitor_collect(&list, &count);

    if (ret != MiniShell_OK)
    {
        fprintf(stderr, "psinfo: failed to collect process information\n");
        return ret;
    }

    print_process_list(list, count);
    process_monitor_free(list);

    return MiniShell_OK;
}
