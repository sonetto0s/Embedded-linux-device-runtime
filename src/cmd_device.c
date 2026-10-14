#include "cmd_device.h"
#include "error.h"
#include "uart_protocol.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEVICE_COMMAND_BUFFER_SIZE 256
#define DEVICE_COMMAND_TIMEOUT_MS 1000

static int device_usage(void)
{
    printf("usage:\n");
    printf("  device list\n");
    printf("  device info|open|close <name>\n");
    printf("  device read <name> [count]\n");
    printf("  device write <name> <byte ...>\n");
    printf("  device ping|status <uart>\n");
    printf("  device heartbeat <uart> <number>\n");
    printf("  device led <uart|led> on|off\n");
    printf("  device led <led> trigger <trigger>\n");
    printf("  device oled <oled> clear|refresh|on|off\n");
    printf("  device oled <oled> text <x> <y> <text ...>\n");
    printf("  device reload\n");
    return MiniShell_ERR_PARSE;
}

static int device_command_error(const char *action, const char *name)
{
    fprintf(stderr, "device: %s %s: %s\n", action, name, strerror(errno));
    return MiniShell_ERR_UNKNOWN;
}

static int device_number(const char *text, unsigned long maximum,
                         int base, unsigned long *number)
{
    if (!text || !*text || *text == '-' || *text == '+')
    {
        return -1;
    }

    char *end;
    errno = 0;
    unsigned long value = strtoul(text, &end, base);

    if (errno == ERANGE || end == text || *end || value > maximum)
    {
        return -1;
    }

    *number = value;
    return 0;
}

static int device_command_list(ShellContext *ctx)
{
    if (ctx->devices.count == 0)
    {
        printf("no devices configured\n");
        return MiniShell_OK;
    }

    printf("Name             Type   State\n");

    for (size_t i = 0; i < ctx->devices.count; i++)
    {
        Device *device = device_manager_get(&ctx->devices, i);
        printf("%-16s %-6s %s\n", device->config.name,
               device_type_string(device->config.type), device_state_string(device->state));
    }

    return MiniShell_OK;
}

static int device_command_info(const Device *device)
{
    const DeviceConfig *config = &device->config;
    printf("Name           : %s\n", config->name);
    printf("Type           : %s\n", device_type_string(config->type));
    printf("State          : %s\n", device_state_string(device->state));
    printf("Opened         : %d\n", device->opened);
    printf("Enabled        : %d\n", config->enabled);
    printf("Last Error     : %s\n", device->last_errno ? strerror(device->last_errno) : "none");

    if (config->type == DEVICE_LED)
    {
        printf("LED Name       : %s\n", config->led_name);
    }
    else
    {
        printf("Path           : %s\n", config->path);
    }

    if (config->type == DEVICE_UART)
    {
        printf("Baudrate       : %d\n", config->baudrate);
    }
    else if (config->type == DEVICE_I2C || config->type == DEVICE_OLED)
    {
        printf("Address        : 0x%02x\n", config->address);
    }
    else if (config->type == DEVICE_GPIO)
    {
        printf("Offset         : %u\n", config->offset);
        printf("Direction      : %s\n", config->gpio_output ? "output" : "input");
        printf("Bias           : %d\n", config->bias);
    }

    return MiniShell_OK;
}

static int device_command_read(Command *cmd, Device *device)
{
    if (device->config.type == DEVICE_GPIO || device->config.type == DEVICE_LED)
    {
        if (cmd->argc != 3)
        {
            return device_usage();
        }

        int value;

        if (device_read(device, &value, sizeof(value)) < 0)
        {
            return device_command_error("read", device->config.name);
        }

        printf("%d\n", value);
        return MiniShell_OK;
    }

    if ((device->config.type != DEVICE_UART && device->config.type != DEVICE_I2C) ||
        cmd->argc > 4)
    {
        return device_usage();
    }

    unsigned long count = 16;

    if (cmd->argc == 4 &&
        (device_number(cmd->argv[3], DEVICE_COMMAND_BUFFER_SIZE, 0, &count) < 0 || count == 0))
    {
        return device_usage();
    }

    if (device->config.type == DEVICE_UART)
    {
        int timeout_ms = DEVICE_COMMAND_TIMEOUT_MS;

        int ready = device_control(device, DEVICE_CONTROL_UART_WAIT, &timeout_ms);

        if (ready <= 0)
        {
            if (ready == 0)
            {
                errno = ETIMEDOUT;
                device->last_errno = errno;
            }
            return device_command_error("read", device->config.name);
        }
    }

    unsigned char buffer[DEVICE_COMMAND_BUFFER_SIZE];
    ssize_t length = device_read(device, buffer, (size_t)count);

    if (length < 0)
    {
        return device_command_error("read", device->config.name);
    }

    for (ssize_t i = 0; i < length; i++)
    {
        printf("%s%02x", i ? " " : "", buffer[i]);
    }

    printf("\n");
    return MiniShell_OK;
}

static int device_command_write(Command *cmd, Device *device)
{
    if (cmd->argc < 4)
    {
        return device_usage();
    }

    if (device->config.type == DEVICE_GPIO || device->config.type == DEVICE_LED)
    {
        unsigned long number;

        if (cmd->argc != 4 || device_number(cmd->argv[3], 1, 0, &number) < 0)
        {
            return device_usage();
        }

        int value = (int)number;

        if (device_write(device, &value, sizeof(value)) < 0)
        {
            return device_command_error("write", device->config.name);
        }

        printf("%s: %d\n", device->config.name, value);
        return MiniShell_OK;
    }

    if (device->config.type != DEVICE_UART && device->config.type != DEVICE_I2C)
    {
        return device_usage();
    }

    unsigned char buffer[DEVICE_COMMAND_BUFFER_SIZE];
    size_t size = (size_t)(cmd->argc - 3);

    if (size > sizeof(buffer))
    {
        return device_usage();
    }

    for (size_t i = 0; i < size; i++)
    {
        unsigned long number;

        if (device_number(cmd->argv[i + 3], UCHAR_MAX, 0, &number) < 0)
        {
            return device_usage();
        }

        buffer[i] = (unsigned char)number;
    }

    if (device->config.type == DEVICE_UART)
    {
        DeviceWriteRequest request = {buffer, size, DEVICE_COMMAND_TIMEOUT_MS};

        if (device_control(device, DEVICE_CONTROL_UART_WRITE_ALL, &request) < 0)
        {
            return device_command_error("write", device->config.name);
        }
    }
    else if (device_write(device, buffer, size) < 0)
    {
        return device_command_error("write", device->config.name);
    }

    printf("%s: wrote %zu bytes\n", device->config.name, size);
    return MiniShell_OK;
}

static int device_command_uart(Device *device, const char *request)
{
    if (device->config.type != DEVICE_UART)
    {
        return device_usage();
    }

    char reply[UART_PROTOCOL_LINE_SIZE];

    if (uart_protocol_request(device, request, reply, sizeof(reply), DEVICE_COMMAND_TIMEOUT_MS) < 0)
    {
        return device_command_error("request", device->config.name);
    }

    printf("%s\n", reply);
    return MiniShell_OK;
}

static int device_command_led(Command *cmd, Device *device)
{
    if (cmd->argc < 4)
    {
        return device_usage();
    }

    const char *action = cmd->argv[3];

    if (device->config.type == DEVICE_UART && cmd->argc == 4)
    {
        if (strcmp(action, "on") == 0)
        {
            return device_command_uart(device, "LED ON");
        }
        if (strcmp(action, "off") == 0)
        {
            return device_command_uart(device, "LED OFF");
        }
    }

    if (device->config.type != DEVICE_LED)
    {
        return device_usage();
    }

    DeviceControl request;
    void *argument = NULL;

    if (strcmp(action, "on") == 0 && cmd->argc == 4)
    {
        request = DEVICE_CONTROL_LED_ON;
    }
    else if (strcmp(action, "off") == 0 && cmd->argc == 4)
    {
        request = DEVICE_CONTROL_LED_OFF;
    }
    else if (strcmp(action, "trigger") == 0 && cmd->argc == 5)
    {
        request = DEVICE_CONTROL_LED_TRIGGER;
        argument = cmd->argv[4];
    }
    else
    {
        return device_usage();
    }

    if (device_control(device, request, argument) < 0)
    {
        return device_command_error(action, device->config.name);
    }

    printf("%s: %s%s%s\n", device->config.name, action,
           argument ? "=" : "", argument ? (const char *)argument : "");
    return MiniShell_OK;
}

static int device_command_oled(Command *cmd, Device *device)
{
    if (device->config.type != DEVICE_OLED || cmd->argc < 4)
    {
        return device_usage();
    }

    const char *action = cmd->argv[3];
    DeviceControl request;
    DeviceText text;
    char buffer[DEVICE_COMMAND_BUFFER_SIZE];
    void *argument = NULL;

    if (cmd->argc == 4 && strcmp(action, "clear") == 0)
    {
        request = DEVICE_CONTROL_OLED_CLEAR;
    }
    else if (cmd->argc == 4 && strcmp(action, "refresh") == 0)
    {
        request = DEVICE_CONTROL_OLED_REFRESH;
    }
    else if (cmd->argc == 4 && strcmp(action, "on") == 0)
    {
        request = DEVICE_CONTROL_OLED_ON;
    }
    else if (cmd->argc == 4 && strcmp(action, "off") == 0)
    {
        request = DEVICE_CONTROL_OLED_OFF;
    }
    else if (cmd->argc >= 7 && strcmp(action, "text") == 0)
    {
        unsigned long x;
        unsigned long y;

        if (device_number(cmd->argv[4], 127, 0, &x) < 0 ||
            device_number(cmd->argv[5], 63, 0, &y) < 0)
        {
            return device_usage();
        }

        size_t length = 0;

        for (int i = 6; i < cmd->argc; i++)
        {
            int written = snprintf(buffer + length, sizeof(buffer) - length,
                                   "%s%s", i == 6 ? "" : " ", cmd->argv[i]);

            if (written < 0 || (size_t)written >= sizeof(buffer) - length)
            {
                return device_usage();
            }

            length += (size_t)written;
        }

        text.x = (int)x;
        text.y = (int)y;
        text.text = buffer;
        argument = &text;
        request = DEVICE_CONTROL_OLED_TEXT;
    }
    else
    {
        return device_usage();
    }

    if (device_control(device, request, argument) < 0)
    {
        return device_command_error(action, device->config.name);
    }

    printf("%s: %s\n", device->config.name, action);
    return MiniShell_OK;
}

int cmd_device(Command *cmd, ShellContext *ctx)
{
    if (!cmd || !ctx || cmd->argc < 2)
    {
        return device_usage();
    }

    const char *action = cmd->argv[1];

    if (strcmp(action, "list") == 0 && cmd->argc == 2)
    {
        return device_command_list(ctx);
    }

    if (strcmp(action, "reload") == 0 && cmd->argc == 2)
    {
        if (!ctx->devices_file[0])
        {
            errno = ENOENT;
            return device_command_error("reload", "devices.conf");
        }

        if (device_manager_load(&ctx->devices, ctx->devices_file) < 0)
        {
            return device_command_error("reload", ctx->devices_file);
        }

        printf("devices config reloaded\n");
        return MiniShell_OK;
    }

    if (cmd->argc < 3)
    {
        return device_usage();
    }

    Device *device = device_manager_find(&ctx->devices, cmd->argv[2]);

    if (!device)
    {
        fprintf(stderr, "device: device not found: %s\n", cmd->argv[2]);
        return MiniShell_ERR_UNKNOWN;
    }

    if (strcmp(action, "info") == 0 && cmd->argc == 3)
    {
        return device_command_info(device);
    }

    if ((strcmp(action, "open") == 0 || strcmp(action, "close") == 0) && cmd->argc == 3)
    {
        int result = strcmp(action, "open") == 0 ? device_open(device) : device_close(device);

        if (result < 0)
        {
            return device_command_error(action, device->config.name);
        }

        printf("%s: %s\n", device->config.name, device_state_string(device->state));
        return MiniShell_OK;
    }

    if (strcmp(action, "read") == 0)
    {
        return device_command_read(cmd, device);
    }

    if (strcmp(action, "write") == 0)
    {
        return device_command_write(cmd, device);
    }

    if (strcmp(action, "ping") == 0 && cmd->argc == 3)
    {
        return device_command_uart(device, "PING");
    }

    if (strcmp(action, "status") == 0 && cmd->argc == 3)
    {
        return device_command_uart(device, "STATUS");
    }

    if (strcmp(action, "heartbeat") == 0 && cmd->argc == 4)
    {
        unsigned long number;

        if (device_number(cmd->argv[3], UINT32_MAX, 10, &number) < 0)
        {
            return device_usage();
        }

        char request[64];
        snprintf(request, sizeof(request), "HEARTBEAT %lu", number);
        return device_command_uart(device, request);
    }

    if (strcmp(action, "led") == 0)
    {
        return device_command_led(cmd, device);
    }

    if (strcmp(action, "oled") == 0)
    {
        return device_command_oled(cmd, device);
    }

    return device_usage();
}
