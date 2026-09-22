#include "process_monitor.h"

#include "error.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void copy_status_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0 || !src)
    {
        return;
    }

    while (*src == ' ' || *src == '\t')
    {
        src++;
    }

    size_t length = strcspn(src, "\r\n");

    if (length >= dst_size)
    {
        length = dst_size - 1;
    }

    memcpy(dst, src, length);
    dst[length] = '\0';
}

int process_monitor_get(pid_t pid, ProcessInfo *info)
{
    if (pid <= 0 || !info)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    char path[64];
    int written = snprintf(path, sizeof(path), "/proc/%ld/status", (long)pid);

    if (written < 0 || (size_t)written >= sizeof(path))
    {
        return MiniShell_ERR_UNKNOWN;
    }

    FILE *fp = fopen(path, "r");

    if (!fp)
    {
        return MiniShell_ERR_OPEN;
    }

    memset(info, 0, sizeof(*info));

    info->pid = pid;
    info->state = '?';

    char line[256];

    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "Name:", 5) == 0)
        {
            copy_status_text(info->name, sizeof(info->name), line + 5);
            continue;
        }

        if (strncmp(line, "State:", 6) == 0)
        {
            const char *value = line + 6;

            while (*value == ' ' || *value == '\t')
            {
                value++;
            }

            if (*value)
            {
                info->state = *value;
            }

            continue;
        }

        if (strncmp(line, "VmRSS:", 6) == 0)
        {
            sscanf(line + 6, "%lu", &info->rss_kb);
            continue;
        }

        if (strncmp(line, "Threads:", 8) == 0)
        {
            sscanf(line + 8, "%u", &info->threads);
        }
    }

    fclose(fp);

    if (info->name[0] == '\0')
    {
        return MiniShell_ERR_UNKNOWN;
    }

    return MiniShell_OK;
}

static int process_info_compare(const void *left, const void *right)
{
    const ProcessInfo *a = left;
    const ProcessInfo *b = right;

    if (a->pid < b->pid)
    {
        return -1;
    }

    if (a->pid > b->pid)
    {
        return 1;
    }

    return 0;
}

int process_monitor_collect(ProcessInfo **list, size_t *count)
{
    if (!list || !count)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    *list = NULL;
    *count = 0;

    DIR *dir = opendir("/proc");

    if (!dir)
    {
        return MiniShell_ERR_OPEN;
    }

    size_t capacity = 64;
    ProcessInfo *result = malloc(capacity * sizeof(*result));

    if (!result)
    {
        closedir(dir);
        return MiniShell_ERR_MEMORY;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (!is_pid_name(entry->d_name))
        {
            continue;
        }

        errno = 0;

        char *end = NULL;
        long value = strtol(entry->d_name, &end, 10);

        if (errno != 0 || !end || *end != '\0' || value <= 0 || value > INT_MAX)
        {
            continue;
        }

        ProcessInfo info;

        if (process_monitor_get((pid_t)value, &info) != MiniShell_OK)
        {
            continue;
        }

        if (*count == capacity)
        {
            size_t new_capacity = capacity * 2;
            ProcessInfo *expanded = realloc(result, new_capacity * sizeof(*expanded));

            if (!expanded)
            {
                free(result);
                closedir(dir);
                return MiniShell_ERR_MEMORY;
            }

            result = expanded;
            capacity = new_capacity;
        }

        result[*count] = info;
        (*count)++;
    }

    closedir(dir);

    if (*count == 0)
    {
        free(result);
        return MiniShell_OK;
    }

    qsort(result, *count, sizeof(*result), process_info_compare);

    ProcessInfo *trimmed = realloc(result, *count * sizeof(*trimmed));

    if (trimmed)
    {
        result = trimmed;
    }

    *list = result;

    return MiniShell_OK;
}

void process_monitor_free(ProcessInfo *list)
{
    free(list);
}



