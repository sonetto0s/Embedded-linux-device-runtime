#include "device_backend.h"

#include "device_config.h"
#include "gpio_line.h"
#include "i2c_bus.h"
#include "led_control.h"
#include "oled.h"
#include "serial_port.h"
#include "sysfs_io.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    I2cBus bus;
    Oled display;
} OledContext;

typedef struct
{
    char brightness_path[DEVICE_PATH_SIZE + DEVICE_NAME_SIZE];
} LedContext;

static void backend_destroy(Device *device)
{
    int saved_errno = errno;

    if (device->ops && device->ops->close)
    {
        device->ops->close(device);
    }

    free(device->context);
    device->context = NULL;
    errno = saved_errno;
}

static int uart_open(Device *device)
{
    return serial_port_open(device->context, device->config.path,
                            device->config.baudrate, 1);
}

static ssize_t uart_read(Device *device, void *buffer, size_t size)
{
    return serial_port_read(device->context, buffer, size);
}

static ssize_t uart_write(Device *device, const void *buffer, size_t size)
{
    return serial_port_write(device->context, buffer, size);
}

static int uart_control(Device *device, DeviceControl request, void *argument)
{
    SerialPort *port = device->context;

    switch (request)
    {
        case DEVICE_CONTROL_UART_WAIT:
            if (!argument)
            {
                errno = EINVAL;
                return -1;
            }

            return serial_port_wait_readable(port, *(int *)argument);
        case DEVICE_CONTROL_UART_WRITE_ALL:
        {
            if (!argument)
            {
                errno = EINVAL;
                return -1;
            }

            const DeviceWriteRequest *write_request = argument;

            return serial_port_write_all(port, write_request->data,
                                         write_request->size, write_request->timeout_ms);
        }
        case DEVICE_CONTROL_UART_FLUSH:
            return serial_port_flush_input(port);
        default:
            errno = ENOTSUP;
            return -1;
    }
}

static int uart_close(Device *device)
{
    serial_port_close(device->context);
    return 0;
}

static int i2c_open(Device *device)
{
    return i2c_bus_open(device->context, device->config.path,
                        (uint8_t)device->config.address);
}

static ssize_t i2c_read(Device *device, void *buffer, size_t size)
{
    if (size > SSIZE_MAX)
    {
        errno = EMSGSIZE;
        return -1;
    }

    if (i2c_bus_read(device->context, buffer, size) < 0)
    {
        return -1;
    }

    return (ssize_t)size;
}

static ssize_t i2c_write(Device *device, const void *buffer, size_t size)
{
    if (size > SSIZE_MAX)
    {
        errno = EMSGSIZE;
        return -1;
    }

    if (i2c_bus_write(device->context, buffer, size) < 0)
    {
        return -1;
    }

    return (ssize_t)size;
}

static int i2c_close(Device *device)
{
    i2c_bus_close(device->context);
    return 0;
}

static int oled_open(Device *device)
{
    OledContext *context = device->context;

    if (i2c_bus_open(&context->bus, device->config.path,
                      (uint8_t)device->config.address) < 0)
    {
        return -1;
    }

    if (oled_init(&context->display, &context->bus) < 0)
    {
        int saved_errno = errno;
        i2c_bus_close(&context->bus);
        context->display.bus = NULL;
        errno = saved_errno;
        return -1;
    }

    return 0;
}

static int oled_control(Device *device, DeviceControl request, void *argument)
{
    OledContext *context = device->context;
    Oled *display = &context->display;

    switch (request)
    {
        case DEVICE_CONTROL_OLED_CLEAR:
            oled_clear(display);
            return 0;
        case DEVICE_CONTROL_OLED_TEXT:
        {
            if (!argument)
            {
                errno = EINVAL;
                return -1;
            }

            const DeviceText *text = argument;

            return oled_draw_text_utf8(display, text->x, text->y, text->text);
        }
        case DEVICE_CONTROL_OLED_REFRESH:
            return oled_refresh(display);
        case DEVICE_CONTROL_OLED_ON:
            return oled_display_on(display);
        case DEVICE_CONTROL_OLED_OFF:
            return oled_display_off(display);
        default:
            errno = ENOTSUP;
            return -1;
    }
}

static int oled_close(Device *device)
{
    OledContext *context = device->context;

    i2c_bus_close(&context->bus);
    context->display.bus = NULL;
    return 0;
}

static int gpio_open(Device *device)
{
    if (device->config.gpio_output)
    {
        return gpio_line_open_output(device->context, device->config.path,
                                      device->config.offset, device->config.initial_value);
    }

    return gpio_line_open_input(device->context, device->config.path,
                                 device->config.offset, (GpioBias)device->config.bias);
}

static ssize_t gpio_read(Device *device, void *buffer, size_t size)
{
    if (!buffer || size != sizeof(int))
    {
        errno = EINVAL;
        return -1;
    }

    int value;

    if (gpio_line_get_value(device->context, &value) < 0)
    {
        return -1;
    }

    memcpy(buffer, &value, sizeof(value));
    return sizeof(value);
}

static ssize_t gpio_write(Device *device, const void *buffer, size_t size)
{
    if (!buffer || size != sizeof(int))
    {
        errno = EINVAL;
        return -1;
    }

    if (!device->config.gpio_output)
    {
        errno = ENOTSUP;
        return -1;
    }

    int value;
    memcpy(&value, buffer, sizeof(value));

    if (gpio_line_set_value(device->context, value) < 0)
    {
        return -1;
    }

    return sizeof(value);
}

static int gpio_close(Device *device)
{
    gpio_line_close(device->context);
    return 0;
}

static int led_open(Device *device)
{
    const LedContext *context = device->context;
    long brightness;

    return sysfs_read_long(context->brightness_path, &brightness);
}

static ssize_t led_read(Device *device, void *buffer, size_t size)
{
    if (!buffer || size != sizeof(int))
    {
        errno = EINVAL;
        return -1;
    }

    const LedContext *context = device->context;
    long brightness;

    if (sysfs_read_long(context->brightness_path, &brightness) < 0)
    {
        return -1;
    }

    int value = brightness != 0;
    memcpy(buffer, &value, sizeof(value));
    return sizeof(value);
}

static int led_set(Device *device, int value)
{
    int result;
    errno = 0;

    if (value == 1)
    {
        result = led_control_set_on(device->config.led_name);
    }
    else if (value == 0)
    {
        result = led_control_set_off(device->config.led_name);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    if (result != 0)
    {
        if (!errno)
        {
            errno = EIO;
        }

        return -1;
    }

    return 0;
}

static ssize_t led_write(Device *device, const void *buffer, size_t size)
{
    if (!buffer || size != sizeof(int))
    {
        errno = EINVAL;
        return -1;
    }

    int value;
    memcpy(&value, buffer, sizeof(value));

    if (led_set(device, value) < 0)
    {
        return -1;
    }

    return sizeof(value);
}

static int led_control(Device *device, DeviceControl request, void *argument)
{
    switch (request)
    {
        case DEVICE_CONTROL_LED_ON:
            return led_set(device, 1);
        case DEVICE_CONTROL_LED_OFF:
            return led_set(device, 0);
        case DEVICE_CONTROL_LED_TRIGGER:
            if (!argument)
            {
                errno = EINVAL;
                return -1;
            }

            errno = 0;

            if (led_control_set_trigger(device->config.led_name, argument) != 0)
            {
                if (!errno)
                {
                    errno = EIO;
                }

                return -1;
            }

            return 0;
        default:
            errno = ENOTSUP;
            return -1;
    }
}

static int led_close(Device *device)
{
    (void)device;
    return 0;
}

static const DeviceOps uart_ops =
{
    uart_open, uart_read, uart_write, uart_control, uart_close, backend_destroy
};

static const DeviceOps i2c_ops =
{
    i2c_open, i2c_read, i2c_write, NULL, i2c_close, backend_destroy
};

static const DeviceOps oled_ops =
{
    oled_open, NULL, NULL, oled_control, oled_close, backend_destroy
};

static const DeviceOps gpio_ops =
{
    gpio_open, gpio_read, gpio_write, NULL, gpio_close, backend_destroy
};

static const DeviceOps led_ops =
{
    led_open, led_read, led_write, led_control, led_close, backend_destroy
};

int device_backend_init(Device *device, const DeviceConfig *config)
{
    if (!device || device_config_validate(config) < 0)
    {
        errno = EINVAL;
        return -1;
    }

    const DeviceOps *ops;
    size_t context_size;

    switch (config->type)
    {
        case DEVICE_UART:
            ops = &uart_ops;
            context_size = sizeof(SerialPort);
            break;
        case DEVICE_I2C:
            ops = &i2c_ops;
            context_size = sizeof(I2cBus);
            break;
        case DEVICE_OLED:
            ops = &oled_ops;
            context_size = sizeof(OledContext);
            break;
        case DEVICE_GPIO:
            ops = &gpio_ops;
            context_size = sizeof(GpioLine);
            break;
        case DEVICE_LED:
            ops = &led_ops;
            context_size = sizeof(LedContext);
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    void *context = calloc(1, context_size);

    if (!context)
    {
        return -1;
    }

    switch (config->type)
    {
        case DEVICE_UART:
            serial_port_init(context);
            break;
        case DEVICE_I2C:
            i2c_bus_init(context);
            break;
        case DEVICE_OLED:
            i2c_bus_init(&((OledContext *)context)->bus);
            break;
        case DEVICE_GPIO:
            gpio_line_init(context);
            break;
        case DEVICE_LED:
        {
            LedContext *led = context;
            int written = snprintf(led->brightness_path, sizeof(led->brightness_path),
                                   "/sys/class/leds/%s/brightness", config->led_name);

            if (written < 0 || (size_t)written >= sizeof(led->brightness_path))
            {
                free(context);
                errno = ENAMETOOLONG;
                return -1;
            }

            break;
        }
        default:
            break;
    }

    device_init(device, config, ops, context);
    return 0;
}
