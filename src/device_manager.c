#include "device_manager.h"
#include "device_backend.h"
#include "device_config.h"
#include <errno.h>
#include <string.h>

void device_manager_init(DeviceManager *manager)
{
    if (manager)
    {
        memset(manager, 0, sizeof(*manager));
    }
}

Device *device_manager_find(DeviceManager *manager, const char *name)
{
    if (!manager || !name)
    {
        return NULL;
    }

    for (size_t i = 0; i < manager->count; i++)
    {
        if (strcmp(manager->devices[i].config.name, name) == 0)
        {
            return &manager->devices[i];
        }
    }

    return NULL;
}

Device *device_manager_get(DeviceManager *manager, size_t index)
{
    if (!manager || index >= manager->count)
    {
        return NULL;
    }

    return &manager->devices[index];
}

int device_manager_register(DeviceManager *manager, const DeviceConfig *config,
                            const DeviceOps *ops, void *context)
{
    if (!manager || !ops || !ops->open || !ops->close ||
        device_config_validate(config) < 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (device_manager_find(manager, config->name))
    {
        errno = EEXIST;
        return -1;
    }

    if (manager->count == DEVICE_MAX_COUNT)
    {
        errno = ENOSPC;
        return -1;
    }

    device_init(&manager->devices[manager->count++], config, ops, context);
    return 0;
}

int device_manager_close_all(DeviceManager *manager)
{
    if (!manager)
    {
        errno = EINVAL;
        return -1;
    }

    int saved_errno = 0;

    for (size_t i = 0; i < manager->count; i++)
    {
        if (device_close(&manager->devices[i]) < 0 && saved_errno == 0)
        {
            saved_errno = errno;
        }
    }

    if (saved_errno)
    {
        errno = saved_errno;
        return -1;
    }

    return 0;
}

void device_manager_destroy(DeviceManager *manager)
{
    if (!manager)
    {
        return;
    }

    for (size_t i = 0; i < manager->count; i++)
    {
        device_destroy(&manager->devices[i]);
    }

    device_manager_init(manager);
}

int device_manager_load(DeviceManager *manager, const char *filename)
{
    if (!manager)
    {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < manager->count; i++)
    {
        if (manager->devices[i].opened)
        {
            errno = EBUSY;
            return -1;
        }
    }

    DeviceConfigList list;

    if (device_config_load(&list, filename) < 0)
    {
        return -1;
    }

    DeviceManager temporary;
    device_manager_init(&temporary);

    for (size_t i = 0; i < list.count; i++)
    {
        if (device_backend_init(&temporary.devices[temporary.count], &list.devices[i]) < 0)
        {
            int saved_errno = errno;
            device_manager_destroy(&temporary);
            errno = saved_errno;
            return -1;
        }
        temporary.count++;
    }

    device_manager_destroy(manager);
    *manager = temporary;
    return 0;
}
