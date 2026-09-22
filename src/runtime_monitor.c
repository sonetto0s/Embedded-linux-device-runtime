#include "runtime_monitor.h"

#include "error.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CPU_SAMPLE_INTERVAL_NS 100000000L

typedef struct
{
    unsigned long long total;
    unsigned long long idle;
} CpuSnapshot;

static int read_cpu_snapshot(CpuSnapshot *snapshot)
{
    if (!snapshot)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    FILE *fp = fopen("/proc/stat", "r");

    if (!fp)
    {
        return MiniShell_ERR_OPEN;
    }

    char line[256];

    if (!fgets(line, sizeof(line), fp))
    {
        fclose(fp);
        return MiniShell_ERR_UNKNOWN;
    }

    fclose(fp);

    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long iowait = 0;
    unsigned long long irq = 0;
    unsigned long long softirq = 0;
    unsigned long long steal = 0;

    int fields = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    if (fields < 4)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    snapshot->idle = idle + iowait;
    snapshot->total = user + nice + system + idle + iowait + irq + softirq + steal;

    if (snapshot->total == 0)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    return MiniShell_OK;
}

static int wait_cpu_sample(void)
{
    struct timespec request = {0, CPU_SAMPLE_INTERVAL_NS};

    while (nanosleep(&request, &request) < 0)
    {
        if (errno != EINTR)
        {
            return MiniShell_ERR_UNKNOWN;
        }
    }

    return MiniShell_OK;
}

static int read_cpu(double *usage)
{
    if (!usage)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    CpuSnapshot first;
    CpuSnapshot second;

    int ret = read_cpu_snapshot(&first);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    ret = wait_cpu_sample();

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    ret = read_cpu_snapshot(&second);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    if (second.total <= first.total || second.idle < first.idle)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    unsigned long long total_delta = second.total - first.total;
    unsigned long long idle_delta = second.idle - first.idle;

    if (idle_delta > total_delta)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    *usage = (double)(total_delta - idle_delta) * 100.0 / (double)total_delta;

    return MiniShell_OK;
}

static int read_memory(double *usage)
{
    if (!usage)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    FILE *fp = fopen("/proc/meminfo", "r");

    if (!fp)
    {
        return MiniShell_ERR_OPEN;
    }

    char line[256];
    unsigned long total = 0;
    unsigned long available = 0;

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "MemTotal: %lu kB", &total) == 1)
        {
            continue;
        }

        if (sscanf(line, "MemAvailable: %lu kB", &available) == 1)
        {
            continue;
        }
    }

    fclose(fp);

    if (total == 0 || available > total)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    *usage = (double)(total - available) * 100.0 / (double)total;

    return MiniShell_OK;
}

static int read_load(double load[3])
{
    if (!load)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    FILE *fp = fopen("/proc/loadavg", "r");

    if (!fp)
    {
        return MiniShell_ERR_OPEN;
    }

    int fields = fscanf(fp, "%lf %lf %lf", &load[0], &load[1], &load[2]);

    fclose(fp);

    return fields == 3 ? MiniShell_OK : MiniShell_ERR_UNKNOWN;
}

static int read_uptime(double *uptime)
{
    if (!uptime)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    FILE *fp = fopen("/proc/uptime", "r");

    if (!fp)
    {
        return MiniShell_ERR_OPEN;
    }

    int fields = fscanf(fp, "%lf", uptime);

    fclose(fp);

    return fields == 1 ? MiniShell_OK : MiniShell_ERR_UNKNOWN;
}

static int is_pid_name(const char *name)
{
    if (!name || !*name)
    {
        return 0;
    }

    for (size_t i = 0; name[i]; i++)
    {
        if (!isdigit((unsigned char)name[i]))
        {
            return 0;
        }
    }

    return 1;
}

static int read_process_count(unsigned int *count)
{
    if (!count)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    DIR *dir = opendir("/proc");

    if (!dir)
    {
        return MiniShell_ERR_OPEN;
    }

    unsigned int result = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (is_pid_name(entry->d_name))
        {
            result++;
        }
    }

    closedir(dir);

    *count = result;

    return MiniShell_OK;
}

int runtime_monitor_collect(RuntimeMonitor *monitor)
{
    if (!monitor)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    memset(monitor, 0, sizeof(*monitor));

    int ret = read_cpu(&monitor->cpu_usage);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    ret = read_memory(&monitor->memory_usage);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    ret = read_load(monitor->load_average);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    ret = read_uptime(&monitor->uptime);

    if (ret != MiniShell_OK)
    {
        return ret;
    }

    return read_process_count(&monitor->process_count);
}




