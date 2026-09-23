#include "i2c_bus.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

void i2c_bus_init(I2cBus *bus)
{
    if (!bus)
    {
        return;
    }

    bus->fd = -1;
    bus->address = 0;
}

int i2c_bus_open(I2cBus *bus, const char *device_path, uint8_t address)
{
    if (!bus || !device_path || address > 0x7F)
    {
        errno = EINVAL;
        return -1;
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0)
    {
        return -1;
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
    {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, address) < 0)
    {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    bus->fd = fd;
    bus->address = address;

    return 0;
}

int i2c_bus_probe(I2cBus *bus)
{
    if (!bus || bus->fd < 0)
    {
        errno = EINVAL;
        return -1;
    }

    struct i2c_smbus_ioctl_data args;

    args.read_write = I2C_SMBUS_WRITE;
    args.command = 0;
    args.size = I2C_SMBUS_QUICK;
    args.data = NULL;

    if (ioctl(bus->fd, I2C_SMBUS, &args) < 0)
    {
        return -1;
    }

    return 0;
}

int i2c_bus_write(I2cBus *bus, const void *data, size_t size)
{
    if (!bus || bus->fd < 0 || (!data && size > 0))
    {
        errno = EINVAL;
        return -1;
    }

    while (1)
    {
        ssize_t ret = write(bus->fd, data, size);

        if (ret < 0 && errno == EINTR)
        {
            continue;
        }

        if (ret < 0)
        {
            return -1;
        }

        if ((size_t)ret != size)
        {
            errno = EIO;
            return -1;
        }

        return 0;
    }
}

int i2c_bus_read(I2cBus *bus, void *data, size_t size)
{
    if (!bus || bus->fd < 0 || (!data && size > 0))
    {
        errno = EINVAL;
        return -1;
    }

    while (1)
    {
        ssize_t ret = read(bus->fd, data, size);

        if (ret < 0 && errno == EINTR)
        {
            continue;
        }

        if (ret < 0)
        {
            return -1;
        }

        if ((size_t)ret != size)
        {
            errno = EIO;
            return -1;
        }

        return 0;
    }
}

void i2c_bus_close(I2cBus *bus)
{
    if (!bus)
    {
        return;
    }

    if (bus->fd >= 0)
    {
        close(bus->fd);
    }

    bus->fd = -1;
    bus->address = 0;
}
