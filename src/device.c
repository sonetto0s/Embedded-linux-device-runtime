#include "device.h"
#include <errno.h>
#include <string.h>

static int device_failure(Device *device)
{
    if (device)
    {
        device->last_errno = errno;
    }

    return -1;
}

static void device_result(Device *device, int failed)
{
    if (!failed)
    {
        device->last_errno = 0;
        device->state = device->opened ? DEVICE_OPEN : DEVICE_CLOSED;
        return;
    }

    device->last_errno = errno;

    if (errno != EAGAIN && errno != EINTR && errno != ETIMEDOUT &&
        errno != EINVAL && errno != ENOTSUP && errno != EPROTO &&
        errno != EMSGSIZE && errno != EBUSY)
    {
        device->state = DEVICE_ERROR;
    }
}

static int device_ready(Device *device)
{
    if (!device || !device->ops)
    {
        errno = EINVAL;
        return device_failure(device);
    }

    if (!device->config.enabled)
    {
        errno = EACCES;
        return device_failure(device);
    }

    if (!device->opened)
    {
        errno = EBADF;
        return device_failure(device);
    }

    if (device->state == DEVICE_ERROR)
    {
        errno = device->last_errno ? device->last_errno : EIO;
        return device_failure(device);
    }

    return 0;
}

void device_init(Device *device, const DeviceConfig *config,
                 const DeviceOps *ops, void *context)
{
    if (!device)
    {
        return;
    }

    memset(device, 0, sizeof(*device));

    if (config)
    {
        device->config = *config;
    }

    device->ops = ops;
    device->context = context;
    device->state = device->config.enabled ? DEVICE_CLOSED : DEVICE_DISABLED;
}

int device_open(Device *device)
{
    if (!device || !device->ops || !device->ops->open)
    {
        errno = EINVAL;
        return device_failure(device);
    }

    if (!device->config.enabled)
    {
        errno = EACCES;
        return device_failure(device);
    }

    if (device->opened)
    {
        if (device->state == DEVICE_ERROR)
        {
            errno = EBUSY;
            return device_failure(device);
        }

        device->last_errno = 0;
        return 0;
    }

    int ret = device->ops->open(device);

    if (ret == 0)
    {
        device->opened = 1;
    }

    device_result(device, ret < 0);
    return ret;
}

ssize_t device_read(Device *device, void *buffer, size_t size)
{
    if (device_ready(device) < 0)
    {
        return -1;
    }

    if (!buffer || size == 0)
    {
        errno = EINVAL;
        return device_failure(device);
    }

    if (!device->ops->read)
    {
        errno = ENOTSUP;
        return device_failure(device);
    }

    ssize_t ret = device->ops->read(device, buffer, size);
    device_result(device, ret < 0);
    return ret;
}

ssize_t device_write(Device *device, const void *buffer, size_t size)
{
    if (device_ready(device) < 0)
    {
        return -1;
    }

    if (!buffer || size == 0)
    {
        errno = EINVAL;
        return device_failure(device);
    }

    if (!device->ops->write)
    {
        errno = ENOTSUP;
        return device_failure(device);
    }

    ssize_t ret = device->ops->write(device, buffer, size);
    device_result(device, ret < 0);
    return ret;
}

int device_control(Device *device, DeviceControl request, void *argument)
{
    if (device_ready(device) < 0)
    {
        return -1;
    }

    if (!device->ops->control)
    {
        errno = ENOTSUP;
        return device_failure(device);
    }

    int ret = device->ops->control(device, request, argument);
    device_result(device, ret < 0);
    return ret;
}

int device_close(Device *device)
{
    if (!device || !device->ops)
    {
        errno = EINVAL;
        return device_failure(device);
    }

    if (!device->opened)
    {
        device->state = device->config.enabled ? DEVICE_CLOSED : DEVICE_DISABLED;
        device->last_errno = 0;
        return 0;
    }

    if (!device->ops->close)
    {
        errno = ENOTSUP;
        return device_failure(device);
    }

    int ret = device->ops->close(device);

    if (ret == 0)
    {
        device->opened = 0;
    }

    device_result(device, ret < 0);
    return ret;
}

void device_destroy(Device *device)
{
    if (!device)
    {
        return;
    }

    if (device->ops)
    {
        device_close(device);

        if (device->ops->destroy)
        {
            device->ops->destroy(device);
        }
    }

    memset(device, 0, sizeof(*device));
}

const char *device_type_string(DeviceType type)
{
    switch (type)
    {
        case DEVICE_UART: return "uart";
        case DEVICE_I2C: return "i2c";
        case DEVICE_OLED: return "oled";
        case DEVICE_GPIO: return "gpio";
        case DEVICE_LED: return "led";
        default: return "unknown";
    }
}

const char *device_state_string(DeviceState state)
{
    switch (state)
    {
        case DEVICE_CLOSED: return "closed";
        case DEVICE_OPEN: return "open";
        case DEVICE_ERROR: return "error";
        case DEVICE_DISABLED: return "disabled";
        default: return "unknown";
    }
}
