#include "watchdog_device.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int write_magic_close(int fd)
{
    const char magic = 'V';

    for (;;)
    {
        ssize_t written = write(fd, &magic, 1);

        if (written == 1)
        {
            return 0;
        }

        if (written < 0 && errno == EINTR)
        {
            continue;
        }

        if (written == 0)
        {
            errno = EIO;
        }

        return -1;
    }
}

void watchdog_device_init(WatchdogDevice *device)
{
    if (!device)
    {
        return;
    }

    memset(device, 0, sizeof(*device));
    device->fd = -1;
}

int watchdog_device_open(WatchdogDevice *device, const char *path)
{
    if (!device || !path || !*path)
    {
        errno = EINVAL;
        return -1;
    }

    if (device->fd >= 0)
    {
        errno = EBUSY;
        return -1;
    }

    int fd = open(path, O_RDWR | O_CLOEXEC);

    if (fd < 0)
    {
        return -1;
    }

    struct watchdog_info info;
    memset(&info, 0, sizeof(info));

    if (ioctl(fd, WDIOC_GETSUPPORT, &info) < 0)
    {
        int saved_errno = errno;
        write_magic_close(fd);
        close(fd);
        errno = saved_errno;
        return -1;
    }

    int timeout;

    if (ioctl(fd, WDIOC_GETTIMEOUT, &timeout) < 0)
    {
        int saved_errno = errno;

        if (info.options & WDIOF_MAGICCLOSE)
        {
            write_magic_close(fd);
        }

        close(fd);
        errno = saved_errno;
        return -1;
    }

    device->fd = fd;
    device->info = info;
    device->timeout = timeout;
    device->has_info = 1;

    return 0;
}

int watchdog_device_get_timeout(WatchdogDevice *device, int *timeout)
{
    if (!device || device->fd < 0 || !timeout)
    {
        errno = EINVAL;
        return -1;
    }

    int value;

    if (ioctl(device->fd, WDIOC_GETTIMEOUT, &value) < 0)
    {
        return -1;
    }

    device->timeout = value;
    *timeout = value;

    return 0;
}

int watchdog_device_set_timeout(WatchdogDevice *device, int timeout)
{
    if (!device || device->fd < 0 || timeout <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    int value = timeout;

    if (ioctl(device->fd, WDIOC_SETTIMEOUT, &value) < 0)
    {
        return -1;
    }

    device->timeout = value;

    return 0;
}

int watchdog_device_keepalive(WatchdogDevice *device)
{
    if (!device || device->fd < 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (ioctl(device->fd, WDIOC_KEEPALIVE, 0) < 0)
    {
        return -1;
    }

    return 0;
}

int watchdog_device_get_timeleft(WatchdogDevice *device, int *timeleft)
{
    if (!device || device->fd < 0 || !timeleft)
    {
        errno = EINVAL;
        return -1;
    }

    int value;

    if (ioctl(device->fd, WDIOC_GETTIMELEFT, &value) < 0)
    {
        return -1;
    }

    *timeleft = value;

    return 0;
}

int watchdog_device_disable(WatchdogDevice *device)
{
    if (!device || device->fd < 0)
    {
        errno = EINVAL;
        return -1;
    }

    int options = WDIOS_DISABLECARD;

    if (ioctl(device->fd, WDIOC_SETOPTIONS, &options) < 0)
    {
        return -1;
    }

    return 0;
}

int watchdog_device_close(WatchdogDevice *device)
{
    if (!device)
    {
        errno = EINVAL;
        return -1;
    }

    if (device->fd < 0)
    {
        return 0;
    }

    int result = 0;
    int saved_errno = 0;

    if (device->has_info && (device->info.options & WDIOF_MAGICCLOSE))
    {
        if (write_magic_close(device->fd) < 0)
        {
            result = -1;
            saved_errno = errno;
        }
    }

    if (close(device->fd) < 0 && result == 0)
    {
        result = -1;
        saved_errno = errno;
    }

    device->fd = -1;
    device->timeout = 0;
    device->has_info = 0;
    memset(&device->info, 0, sizeof(device->info));

    if (result < 0)
    {
        errno = saved_errno;
    }

    return result;
}
