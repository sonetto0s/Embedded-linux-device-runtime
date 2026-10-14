#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "device.h"

typedef struct
{
    DeviceConfig devices[DEVICE_MAX_COUNT];
    size_t count;
} DeviceConfigList;

void device_config_init(DeviceConfig *config);
int device_config_validate(const DeviceConfig *config);
int device_config_load(DeviceConfigList *configs, const char *filename);

#endif
