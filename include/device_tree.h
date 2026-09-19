#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

#include <stddef.h>

#define DEVICE_TREE_MAX_COMPATIBLE 8

typedef struct
{
    int available;
    char model[128];
    char compatible[DEVICE_TREE_MAX_COMPATIBLE][128];
    size_t compatible_count;
    char bootargs[1024];
} DeviceTreeInfo;

int device_tree_collect(DeviceTreeInfo *info);
void device_tree_print(const DeviceTreeInfo *info);

#endif
