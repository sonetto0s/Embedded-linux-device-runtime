#include "thermal_monitor.h"

#include "error.h"
#include "sysfs_io.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0 || !src)
    {
        return;
    }

    size_t length = strlen(src);

    if (length >= dst_size)
    {
        length = dst_size - 1;
    }

    memcpy(dst, src, length);
    dst[length] = '\0';
}

static int build_path(char *buffer, size_t size, const char *base, const char *name, const char *file)
{
    int written = snprintf(buffer, size, "%s/%s/%s", base, name, file);

    if (written < 0 || (size_t)written >= size)
    {
        return -1;
    }

    return 0;
}

static int thermal_info_compare(const void *left, const void *right)
{
    const ThermalRuntimeInfo *a = left;
    const ThermalRuntimeInfo *b = right;

    return strcmp(a->name, b->name);
}

int thermal_monitor_collect_from(ThermalMonitor *monitor, const char *base)
{
    if (!monitor || !base || !*base)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    memset(monitor, 0, sizeof(*monitor));

    DIR *dir = opendir(base);

    if (!dir)
    {
        return MiniShell_ERR_OPEN;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "thermal_zone", sizeof("thermal_zone") - 1) != 0)
        {
            continue;
        }

        if (monitor->count >= THERMAL_MONITOR_MAX_ZONES)
        {
            break;
        }

        ThermalRuntimeInfo *info = &monitor->zones[monitor->count];
        char path[512];

        copy_text(info->name, sizeof(info->name), entry->d_name);

        if (build_path(path, sizeof(path), base, entry->d_name, "type") == 0)
        {
            sysfs_read_text(path, info->type, sizeof(info->type));
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "temp") == 0)
        {
            long temperature;

            if (sysfs_read_long(path, &temperature) == 0)
            {
                info->temperature = (double)temperature / 1000.0;
                info->has_temperature = 1;
            }
        }

        monitor->count++;
    }

    closedir(dir);

    if (monitor->count > 1)
    {
        qsort(monitor->zones, monitor->count, sizeof(monitor->zones[0]), thermal_info_compare);
    }

    return MiniShell_OK;
}

int thermal_monitor_collect(ThermalMonitor *monitor)
{
    return thermal_monitor_collect_from(monitor, "/sys/class/thermal");
}

const ThermalRuntimeInfo *thermal_monitor_hottest(const ThermalMonitor *monitor)
{
    if (!monitor)
    {
        return NULL;
    }

    const ThermalRuntimeInfo *hottest = NULL;

    for (size_t i = 0; i < monitor->count; i++)
    {
        const ThermalRuntimeInfo *info = &monitor->zones[i];

        if (!info->has_temperature)
        {
            continue;
        }

        if (!hottest || info->temperature > hottest->temperature)
        {
            hottest = info;
        }
    }

    return hottest;
}
