#include "gpio_line.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef GPIOHANDLE_REQUEST_BIAS_PULL_UP
#define GPIOHANDLE_REQUEST_BIAS_PULL_UP (1UL << 5)
#endif

#ifndef GPIOHANDLE_REQUEST_BIAS_PULL_DOWN
#define GPIOHANDLE_REQUEST_BIAS_PULL_DOWN (1UL << 6)
#endif

#ifndef GPIOHANDLE_REQUEST_BIAS_DISABLE
#define GPIOHANDLE_REQUEST_BIAS_DISABLE (1UL << 7)
#endif

static int gpio_bias_flags(GpioBias bias, unsigned long *flags)
{
    if (!flags)
    {
        errno = EINVAL;
        return -1;
    }

    switch (bias)
    {
        case GPIO_BIAS_DEFAULT:
            *flags = 0;
            return 0;
        case GPIO_BIAS_PULL_UP:
            *flags = GPIOHANDLE_REQUEST_BIAS_PULL_UP;
            return 0;
        case GPIO_BIAS_PULL_DOWN:
            *flags = GPIOHANDLE_REQUEST_BIAS_PULL_DOWN;
            return 0;
        case GPIO_BIAS_DISABLED:
            *flags = GPIOHANDLE_REQUEST_BIAS_DISABLE;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

static int gpio_line_request(GpioLine *line, const char *chip_path, unsigned int offset,
                             unsigned long flags, int default_value)
{
    if (!line || !chip_path || !*chip_path)
    {
        errno = EINVAL;
        return -1;
    }

    if (line->fd >= 0)
    {
        errno = EBUSY;
        return -1;
    }

    int chip_fd = open(chip_path, O_RDONLY | O_CLOEXEC);

    if (chip_fd < 0)
    {
        return -1;
    }

    struct gpiohandle_request request;
    memset(&request, 0, sizeof(request));

    request.lineoffsets[0] = offset;
    request.flags = flags;
    request.default_values[0] = default_value ? 1 : 0;
    request.lines = 1;

    snprintf(request.consumer_label, sizeof(request.consumer_label), "embedded-runtime");

    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &request) < 0)
    {
        int saved_errno = errno;
        close(chip_fd);
        errno = saved_errno;
        return -1;
    }

    if (close(chip_fd) < 0)
    {
        int saved_errno = errno;
        close(request.fd);
        errno = saved_errno;
        return -1;
    }

    line->fd = request.fd;
    return 0;
}

void gpio_line_init(GpioLine *line)
{
    if (line)
    {
        line->fd = -1;
    }
}

int gpio_line_open_input(GpioLine *line, const char *chip_path, unsigned int offset, GpioBias bias)
{
    unsigned long bias_flags;

    if (gpio_bias_flags(bias, &bias_flags) < 0)
    {
        return -1;
    }

    return gpio_line_request(line, chip_path, offset, GPIOHANDLE_REQUEST_INPUT | bias_flags, 0);
}

int gpio_line_open_output(GpioLine *line, const char *chip_path, unsigned int offset, int initial_value)
{
    if (initial_value != 0 && initial_value != 1)
    {
        errno = EINVAL;
        return -1;
    }

    return gpio_line_request(line, chip_path, offset, GPIOHANDLE_REQUEST_OUTPUT, initial_value);
}

int gpio_line_get_value(const GpioLine *line, int *value)
{
    if (!line || line->fd < 0 || !value)
    {
        errno = EINVAL;
        return -1;
    }

    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));

    if (ioctl(line->fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0)
    {
        return -1;
    }

    *value = data.values[0] ? 1 : 0;
    return 0;
}

int gpio_line_set_value(const GpioLine *line, int value)
{
    if (!line || line->fd < 0 || (value != 0 && value != 1))
    {
        errno = EINVAL;
        return -1;
    }

    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));

    data.values[0] = value;

    return ioctl(line->fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
}

void gpio_line_close(GpioLine *line)
{
    if (!line)
    {
        return;
    }

    if (line->fd >= 0)
    {
        close(line->fd);
        line->fd = -1;
    }
}

