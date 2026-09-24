#ifndef WATCHDOG_DEVICE_H
#define WATCHDOG_DEVICE_H

#include <linux/watchdog.h>

typedef struct
{
    int fd;
    struct watchdog_info info;
    int timeout;
    int has_info;
} WatchdogDevice;

void watchdog_device_init(WatchdogDevice *device);
int watchdog_device_open(WatchdogDevice *device, const char *path);
int watchdog_device_get_timeout(WatchdogDevice *device, int *timeout);
int watchdog_device_set_timeout(WatchdogDevice *device, int timeout);
int watchdog_device_keepalive(WatchdogDevice *device);
int watchdog_device_get_timeleft(WatchdogDevice *device, int *timeleft);
int watchdog_device_disable(WatchdogDevice *device);
int watchdog_device_close(WatchdogDevice *device);

#endif
