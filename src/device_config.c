#include "device_config.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEVICE_CONFIG_LINE_SIZE 512

enum
{
    CONFIG_TYPE = 1U << 0,
    CONFIG_PATH = 1U << 1,
    CONFIG_ENABLED = 1U << 2,
    CONFIG_BAUDRATE = 1U << 3,
    CONFIG_ADDRESS = 1U << 4,
    CONFIG_OFFSET = 1U << 5,
    CONFIG_DIRECTION = 1U << 6,
    CONFIG_BIAS = 1U << 7,
    CONFIG_INITIAL = 1U << 8,
    CONFIG_LED_NAME = 1U << 9
};

static int config_invalid(void)
{
    errno = EINVAL;
    return -1;
}

static int config_name_valid(const char *name)
{
    if (!name || !*name)
    {
        return 0;
    }

    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
    {
        if (!isalnum(*p) && *p != '_' && *p != '-')
        {
            return 0;
        }
    }

    return 1;
}

static int config_led_name_valid(const char *name)
{
    if (!*name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        return 0;
    }

    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
    {
        if (*p == '/' || isspace(*p) || *p < 0x20 || *p == 0x7f)
        {
            return 0;
        }
    }

    return 1;
}

void device_config_init(DeviceConfig *config)
{
    if (!config)
    {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->type = DEVICE_UART;
    config->enabled = 1;
    config->baudrate = 115200;
}

int device_config_validate(const DeviceConfig *config)
{
    if (!config || !memchr(config->name, '\0', sizeof(config->name)) ||
        !memchr(config->path, '\0', sizeof(config->path)) ||
        !memchr(config->led_name, '\0', sizeof(config->led_name)))
    {
        return config_invalid();
    }

    if (!config_name_valid(config->name) ||
        (config->enabled != 0 && config->enabled != 1) ||
        config->type < DEVICE_UART || config->type > DEVICE_LED)
    {
        return config_invalid();
    }

    if (config->type == DEVICE_LED)
    {
        return config_led_name_valid(config->led_name) ? 0 : config_invalid();
    }

    if (config->path[0] != '/' || config->path[1] == '\0')
    {
        return config_invalid();
    }

    if (config->type == DEVICE_UART)
    {
        switch (config->baudrate)
        {
            case 9600:
            case 19200:
            case 38400:
            case 57600:
            case 115200:
            case 230400:
                return 0;
            default:
                return config_invalid();
        }
    }

    if ((config->type == DEVICE_I2C || config->type == DEVICE_OLED) &&
        (config->address < 0x03 || config->address > 0x77))
    {
        return config_invalid();
    }

    if (config->type == DEVICE_GPIO &&
        ((config->gpio_output != 0 && config->gpio_output != 1) ||
         (config->initial_value != 0 && config->initial_value != 1) ||
         config->bias < 0 || config->bias > 3 ||
         (config->gpio_output && config->bias != 0)))
    {
        return config_invalid();
    }

    return 0;
}

static char *config_trim(char *text)
{
    while (isspace((unsigned char)*text))
    {
        text++;
    }

    size_t length = strlen(text);

    while (length > 0 && isspace((unsigned char)text[length - 1]))
    {
        text[--length] = '\0';
    }

    return text;
}

static int config_copy(char *target, size_t size, const char *value)
{
    size_t length = strlen(value);

    if (length == 0 || length >= size)
    {
        return config_invalid();
    }

    memcpy(target, value, length + 1);
    return 0;
}

static int config_unsigned(const char *value, unsigned long maximum,
                           unsigned long *number)
{
    if (!*value || *value == '-' || *value == '+' ||
        isspace((unsigned char)*value))
    {
        return config_invalid();
    }

    char *end;
    errno = 0;
    unsigned long parsed = strtoul(value, &end, 0);

    if (errno == ERANGE || *end != '\0' || parsed > maximum)
    {
        return config_invalid();
    }

    *number = parsed;
    return 0;
}

static int config_field(DeviceConfig *config, unsigned int *fields,
                        const char *key, const char *value)
{
    static const char *keys[] =
    {
        "type", "path", "enabled", "baudrate", "address", "offset",
        "direction", "bias", "initial_value", "led_name"
    };
    size_t index;

    for (index = 0; index < sizeof(keys) / sizeof(keys[0]); index++)
    {
        if (strcmp(key, keys[index]) == 0)
        {
            break;
        }
    }

    if (index == sizeof(keys) / sizeof(keys[0]) ||
        (*fields & (1U << index)) != 0 || !*value)
    {
        return config_invalid();
    }

    *fields |= 1U << index;
    unsigned long number;

    switch (1U << index)
    {
        case CONFIG_TYPE:
            for (int type = DEVICE_UART; type <= DEVICE_LED; type++)
            {
                if (strcmp(value, device_type_string((DeviceType)type)) == 0)
                {
                    config->type = (DeviceType)type;
                    return 0;
                }
            }
            return config_invalid();
        case CONFIG_PATH:
            return config_copy(config->path, sizeof(config->path), value);
        case CONFIG_LED_NAME:
            return config_copy(config->led_name, sizeof(config->led_name), value);
        case CONFIG_ENABLED:
        case CONFIG_INITIAL:
            if (config_unsigned(value, 1, &number) < 0)
            {
                return -1;
            }
            if (index == 2)
            {
                config->enabled = (int)number;
            }
            else
            {
                config->initial_value = (int)number;
            }
            return 0;
        case CONFIG_BAUDRATE:
            if (config_unsigned(value, INT_MAX, &number) < 0)
            {
                return -1;
            }
            config->baudrate = (int)number;
            return 0;
        case CONFIG_ADDRESS:
        case CONFIG_OFFSET:
            if (config_unsigned(value, UINT_MAX, &number) < 0)
            {
                return -1;
            }
            if (index == 4)
            {
                config->address = (unsigned int)number;
            }
            else
            {
                config->offset = (unsigned int)number;
            }
            return 0;
        case CONFIG_DIRECTION:
            if (strcmp(value, "input") != 0 && strcmp(value, "output") != 0)
            {
                return config_invalid();
            }
            config->gpio_output = strcmp(value, "output") == 0;
            return 0;
        case CONFIG_BIAS:
        {
            static const char *biases[] = {"default", "pull_up", "pull_down", "disabled"};
            for (int bias = 0; bias < 4; bias++)
            {
                if (strcmp(value, biases[bias]) == 0)
                {
                    config->bias = bias;
                    return 0;
                }
            }
            return config_invalid();
        }
        default:
            return config_invalid();
    }
}

static int config_section_validate(const DeviceConfig *config, unsigned int fields)
{
    unsigned int required = CONFIG_TYPE;
    unsigned int allowed = CONFIG_TYPE | CONFIG_ENABLED;

    switch (config->type)
    {
        case DEVICE_UART:
            required |= CONFIG_PATH;
            allowed |= CONFIG_PATH | CONFIG_BAUDRATE;
            break;
        case DEVICE_I2C:
        case DEVICE_OLED:
            required |= CONFIG_PATH | CONFIG_ADDRESS;
            allowed |= CONFIG_PATH | CONFIG_ADDRESS;
            break;
        case DEVICE_GPIO:
            required |= CONFIG_PATH | CONFIG_OFFSET;
            allowed |= CONFIG_PATH | CONFIG_OFFSET | CONFIG_DIRECTION;
            if (config->gpio_output)
            {
                allowed |= CONFIG_INITIAL;
            }
            else
            {
                allowed |= CONFIG_BIAS;
            }
            break;
        case DEVICE_LED:
            required |= CONFIG_LED_NAME;
            allowed |= CONFIG_LED_NAME;
            break;
        default:
            return config_invalid();
    }

    if ((fields & required) != required || (fields & ~allowed) != 0)
    {
        return config_invalid();
    }

    return device_config_validate(config);
}

static int config_read_line(FILE *file, char *line, size_t size)
{
    size_t length = 0;
    int character;

    while ((character = fgetc(file)) != EOF && character != '\n')
    {
        if (character == '\0' || length + 1 >= size)
        {
            return config_invalid();
        }
        line[length++] = (char)character;
    }

    if (ferror(file))
    {
        errno = EIO;
        return -1;
    }

    line[length] = '\0';
    return character == EOF && length == 0 ? 0 : 1;
}

int device_config_load(DeviceConfigList *list, const char *filename)
{
    if (!list || !filename || !*filename)
    {
        return config_invalid();
    }

    FILE *file = fopen(filename, "rb");

    if (!file)
    {
        return -1;
    }

    DeviceConfigList temporary;
    memset(&temporary, 0, sizeof(temporary));
    unsigned int fields = 0;
    char line[DEVICE_CONFIG_LINE_SIZE];
    int result;

    while ((result = config_read_line(file, line, sizeof(line))) > 0)
    {
        char *text = config_trim(line);

        if (!*text || *text == '#' || *text == ';')
        {
            continue;
        }

        if (*text == '[')
        {
            size_t length = strlen(text);

            if (length < 3 || text[length - 1] != ']' ||
                (temporary.count > 0 &&
                 config_section_validate(&temporary.devices[temporary.count - 1], fields) < 0))
            {
                result = config_invalid();
                break;
            }

            text[length - 1] = '\0';
            const char *name = text + 1;

            if (!config_name_valid(name) || strlen(name) >= DEVICE_NAME_SIZE)
            {
                result = config_invalid();
                break;
            }

            for (size_t i = 0; i < temporary.count; i++)
            {
                if (strcmp(temporary.devices[i].name, name) == 0)
                {
                    errno = EEXIST;
                    result = -1;
                    break;
                }
            }

            if (result < 0)
            {
                break;
            }

            if (temporary.count == DEVICE_MAX_COUNT)
            {
                errno = ENOSPC;
                result = -1;
                break;
            }

            DeviceConfig *config = &temporary.devices[temporary.count++];
            device_config_init(config);
            strcpy(config->name, name);
            fields = 0;
            continue;
        }

        char *equals = strchr(text, '=');

        if (temporary.count == 0 || !equals)
        {
            result = config_invalid();
            break;
        }

        *equals = '\0';

        if (config_field(&temporary.devices[temporary.count - 1], &fields,
                         config_trim(text), config_trim(equals + 1)) < 0)
        {
            result = -1;
            break;
        }
    }

    if (result >= 0 && temporary.count > 0)
    {
        result = config_section_validate(&temporary.devices[temporary.count - 1], fields);
    }

    int saved_errno = errno;

    if (fclose(file) != 0 && result >= 0)
    {
        result = -1;
        saved_errno = errno;
    }

    if (result < 0)
    {
        errno = saved_errno;
        return -1;
    }

    *list = temporary;
    return 0;
}
