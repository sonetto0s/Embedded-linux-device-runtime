#include "led_control.h"
#include "error.h"
#include "log.h"
#include "sysfs_io.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define LED_SYSFS_BASE "/sys/class/leds"
#define LED_PATH_SIZE 512
#define LED_TRIGGER_BUFFER_SIZE 2048

static int led_name_valid(const char *name)
{
    if (!name || !*name)
    {
        return 0;
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        return 0;
    }

    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
    {
        if (*p == '/' || *p < 0x20 || *p == 0x7f)
        {
            return 0;
        }
    }

    return 1;
}

static int led_trigger_valid(const char *trigger)
{
    if (!trigger || !*trigger)
    {
        return 0;
    }

    for (const unsigned char *p = (const unsigned char *)trigger; *p; p++)
    {
        if (*p == '/' || isspace(*p) || *p < 0x20 || *p == 0x7f)
        {
            return 0;
        }
    }

    return 1;
}

static int led_build_path(char *buffer, size_t size, const char *name, const char *attribute)
{
    if (!buffer || size == 0 || !led_name_valid(name) || !attribute || !*attribute)
    {
        errno = EINVAL;
        return -1;
    }

    int written = snprintf(buffer, size, "%s/%s/%s", LED_SYSFS_BASE, name, attribute);

    if (written < 0 || (size_t)written >= size)
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

static int led_read_active_trigger(const char *name, char *trigger, size_t size)
{
    if (!trigger || size == 0)
    {
        errno = EINVAL;
        return -1;
    }

    char path[LED_PATH_SIZE];

    if (led_build_path(path, sizeof(path), name, "trigger") < 0)
    {
        return -1;
    }

    char buffer[LED_TRIGGER_BUFFER_SIZE];

    if (sysfs_read_text(path, buffer, sizeof(buffer)) < 0)
    {
        return -1;
    }

    char *begin = strchr(buffer, '[');

    if (!begin)
    {
        errno = EIO;
        return -1;
    }

    char *end = strchr(begin, ']');

    if (!end || end <= begin + 1)
    {
        errno = EIO;
        return -1;
    }

    size_t length = (size_t)(end - begin - 1);

    if (length >= size)
    {
        errno = EOVERFLOW;
        return -1;
    }

    memcpy(trigger, begin + 1, length);
    trigger[length] = '\0';

    return 0;
}

static int led_write_trigger(const char *name, const char *trigger)
{
    char path[LED_PATH_SIZE];

    if (led_build_path(path, sizeof(path), name, "trigger") < 0)
    {
        return -1;
    }

    if (sysfs_write_text(path, trigger) < 0)
    {
        return -1;
    }

    char actual[64];

    if (led_read_active_trigger(name, actual, sizeof(actual)) < 0)
    {
        return -1;
    }

    if (strcmp(actual, trigger) != 0)
    {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int led_read_max_brightness(const char *name, long *value)
{
    if (!value)
    {
        errno = EINVAL;
        return -1;
    }

    char path[LED_PATH_SIZE];

    if (led_build_path(path, sizeof(path), name, "max_brightness") < 0)
    {
        return -1;
    }

    if (sysfs_read_long(path, value) < 0)
    {
        return -1;
    }

    if (*value <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static int led_write_brightness(const char *name, long value)
{
    char path[LED_PATH_SIZE];

    if (led_build_path(path, sizeof(path), name, "brightness") < 0)
    {
        return -1;
    }

    char text[32];
    int written = snprintf(text, sizeof(text), "%ld", value);

    if (written < 0 || (size_t)written >= sizeof(text))
    {
        errno = EINVAL;
        return -1;
    }

    if (sysfs_write_text(path, text) < 0)
    {
        return -1;
    }

    long actual;

    if (sysfs_read_long(path, &actual) < 0)
    {
        return -1;
    }

    if (actual != value)
    {
        errno = EIO;
        return -1;
    }

    return 0;
}

int led_control_set_on(const char *name)
{
    if (!led_name_valid(name))
    {
        log_error("invalid LED name");
        return MiniShell_ERR_UNKNOWN;
    }

    long max_brightness;

    if (led_read_max_brightness(name, &max_brightness) < 0)
    {
        log_error("failed read LED max brightness: %s", strerror(errno));
        return MiniShell_ERR_UNKNOWN;
    }

    if (led_write_trigger(name, "none") < 0)
    {
        log_error("failed set LED manual mode: %s", strerror(errno));
        return MiniShell_ERR_UNKNOWN;
    }

    if (led_write_brightness(name, max_brightness) < 0)
    {
        log_error("failed turn LED on: %s", strerror(errno));
        return MiniShell_ERR_UNKNOWN;
    }

    return MiniShell_OK;
}

int led_control_set_off(const char *name)
{
    if (!led_name_valid(name))
    {
        log_error("invalid LED name");
        return MiniShell_ERR_UNKNOWN;
    }

    if (led_write_trigger(name, "none") < 0)
    {
        log_error("failed set LED manual mode: %s", strerror(errno));
        return MiniShell_ERR_UNKNOWN;
    }

    if (led_write_brightness(name, 0) < 0)
    {
        log_error("failed turn LED off: %s", strerror(errno));
        return MiniShell_ERR_UNKNOWN;
    }

    return MiniShell_OK;
}

int led_control_set_trigger(const char *name, const char *trigger)
{
    if (!led_name_valid(name) || !led_trigger_valid(trigger))
    {
        log_error("invalid LED trigger");
        return MiniShell_ERR_UNKNOWN;
    }

    if (led_write_trigger(name, trigger) < 0)
    {
        log_error("failed set LED trigger: %s", strerror(errno));
        return MiniShell_ERR_UNKNOWN;
    }

    return MiniShell_OK;
}












