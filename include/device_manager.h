#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "device_config.h"

typedef struct
{
    Device devices[DEVICE_MAX_COUNT];
    size_t count;
} DeviceManager;

void device_manager_init(DeviceManager *manager);
int device_manager_register(DeviceManager *manager, const DeviceConfig *config,
                            const DeviceOps *ops, void *context);
Device *device_manager_find(DeviceManager *manager, const char *name);
Device *device_manager_get(DeviceManager *manager, size_t index);
int device_manager_load(DeviceManager *manager, const char *filename);
int device_manager_close_all(DeviceManager *manager);
void device_manager_destroy(DeviceManager *manager);

#endif
