#include "device_tree.h"
#include "error.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static size_t read_property(const char *path, unsigned char *buffer, size_t size)
{
    if (!path || !buffer || size == 0)
    {
        return 0;
    }

    FILE *fp = fopen(path, "rb");

    if (!fp)
    {
        return 0;
    }

    size_t length = fread(buffer, 1, size, fp);

    fclose(fp);

    return length;
}



static void copy_property_text(char *dst, size_t dst_size, const unsigned char *src, size_t src_size)
{
    if (!dst || dst_size == 0 || !src)
    {
        return;
    }

    size_t length = 0;

    while (length < src_size && src[length] != '\0' && src[length] != '\r' && src[length] != '\n')
    {
        length++;
    }

    if (length >= dst_size)
    {
        length = dst_size - 1;
    }

    memcpy(dst, src, length);
    dst[length] = '\0';
}


static void collect_model(DeviceTreeInfo *info)
{
    unsigned char buffer[256];
    size_t length = read_property("/proc/device-tree/model", buffer, sizeof(buffer));

    if (length == 0)
    {
        return;
    }

    copy_property_text(info->model, sizeof(info->model), buffer, length);
}

static void collect_compatible(DeviceTreeInfo *info)
{
    unsigned char buffer[1024];
    size_t length = read_property("/proc/device-tree/compatible", buffer, sizeof(buffer));

    if (length == 0)
    {
        return;
    }

    size_t offset = 0;

    while (offset < length && info->compatible_count < DEVICE_TREE_MAX_COMPATIBLE)
    {
        size_t start = offset;

        while (offset < length && buffer[offset] != '\0')
        {
            offset++;
        }

        size_t item_length = offset - start;

        if (item_length > 0)
        {
            char *dst = info->compatible[info->compatible_count];
            size_t dst_size = sizeof(info->compatible[info->compatible_count]);

            if (item_length >= dst_size)
            {
                item_length = dst_size - 1;
            }

            memcpy(dst, buffer + start, item_length);
            dst[item_length] = '\0';
            info->compatible_count++;
        }

        if (offset < length)
        {
            offset++;
        }
    }
}

static void collect_bootargs(DeviceTreeInfo *info)
{
    unsigned char buffer[sizeof(info->bootargs)];
    size_t length = read_property("/proc/device-tree/chosen/bootargs", buffer, sizeof(buffer));

    if (length == 0)
    {
        return;
    }

    copy_property_text(info->bootargs, sizeof(info->bootargs), buffer, length);
}

int device_tree_collect(DeviceTreeInfo *info)
{
    if (!info)
    {
        log_error("device_tree info is null");
        return MiniShell_ERR_UNKNOWN;
    }

    memset(info, 0, sizeof(*info));

    if (access("/proc/device-tree", F_OK) < 0)
    {
        return MiniShell_OK;
    }

    info->available = 1;

    collect_model(info);
    collect_compatible(info);
    collect_bootargs(info);

    return MiniShell_OK;
}



void device_tree_print(const DeviceTreeInfo *info)
{
    if (!info)
    {
        log_error("device_tree info is null");
        return;
    }

    printf("\n");
    printf("======= Device Tree =======\n");

    if (!info->available)
    {
        printf("Status           : unavailable\n");
        printf("\n");
        return;
    }

    printf("Model            : %s\n", info->model[0] ? info->model : "N/A");

    printf("Compatible       :");

    if (info->compatible_count == 0)
    {
        printf(" N/A\n");
    }
    else
    {
        printf("\n");

        for (size_t i = 0; i < info->compatible_count; i++)
        {
            printf("  %s\n", info->compatible[i]);
        }
    }

    printf("Boot Args        : %s\n", info->bootargs[0] ? info->bootargs : "N/A");
    printf("\n");
}




