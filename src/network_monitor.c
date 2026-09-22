#include "network_monitor.h"

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

static int network_info_compare(const void *left, const void *right)
{
    const NetworkRuntimeInfo *a = left;
    const NetworkRuntimeInfo *b = right;

    return strcmp(a->name, b->name);
}

int network_monitor_collect_from(NetworkMonitor *monitor, const char *base)
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
        if (entry->d_name[0] == '.')
        {
            continue;
        }

        if (monitor->count >= NETWORK_MONITOR_MAX_INTERFACES)
        {
            break;
        }

        NetworkRuntimeInfo *info = &monitor->interfaces[monitor->count];
        char path[512];

        copy_text(info->name, sizeof(info->name), entry->d_name);

        if (build_path(path, sizeof(path), base, entry->d_name, "operstate") == 0)
        {
            sysfs_read_text(path, info->state, sizeof(info->state));
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "statistics/rx_bytes") == 0)
        {
            if (sysfs_read_ull(path, &info->rx_bytes) == 0)
            {
                info->has_rx = 1;
            }
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "statistics/tx_bytes") == 0)
        {
            if (sysfs_read_ull(path, &info->tx_bytes) == 0)
            {
                info->has_tx = 1;
            }
        }

        monitor->count++;
    }

    closedir(dir);

    if (monitor->count > 1)
    {
        qsort(monitor->interfaces, monitor->count, sizeof(monitor->interfaces[0]), network_info_compare);
    }

    return MiniShell_OK;
}

int network_monitor_collect(NetworkMonitor *monitor)
{
    return network_monitor_collect_from(monitor, "/sys/class/net");
}

const NetworkRuntimeInfo *network_monitor_primary(const NetworkMonitor *monitor)
{
    if (!monitor || monitor->count == 0)
    {
        return NULL;
    }

    for (size_t i = 0; i < monitor->count; i++)
    {
        const NetworkRuntimeInfo *info = &monitor->interfaces[i];

        if (strcmp(info->name, "lo") != 0 && strcmp(info->state, "up") == 0)
        {
            return info;
        }
    }

    for (size_t i = 0; i < monitor->count; i++)
    {
        if (strcmp(monitor->interfaces[i].name, "lo") != 0)
        {
            return &monitor->interfaces[i];
        }
    }

    return &monitor->interfaces[0];
}
