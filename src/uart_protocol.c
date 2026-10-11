#include "uart_protocol.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef enum
{
    UART_REQUEST_PING = 0,
    UART_REQUEST_STATUS,
    UART_REQUEST_LED_ON,
    UART_REQUEST_LED_OFF,
    UART_REQUEST_HEARTBEAT
} UartRequest;

static int parse_unsigned(const char *text, uint32_t *value)
{
    if (!text || !*text || !value)
    {
        return -1;
    }

    uint32_t result = 0;

    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
    {
        if (*p < '0' || *p > '9')
        {
            return -1;
        }

        unsigned int digit = *p - '0';

        if (result > (UINT32_MAX - digit) / 10U)
        {
            return -1;
        }

        result = result * 10U + digit;
    }

    *value = result;
    return 0;
}

static int parse_request(const char *text, UartRequest *request, uint32_t *heartbeat)
{
    if (strcmp(text, "PING") == 0)
    {
        *request = UART_REQUEST_PING;
    }
    else if (strcmp(text, "STATUS") == 0)
    {
        *request = UART_REQUEST_STATUS;
    }
    else if (strcmp(text, "LED ON") == 0)
    {
        *request = UART_REQUEST_LED_ON;
    }
    else if (strcmp(text, "LED OFF") == 0)
    {
        *request = UART_REQUEST_LED_OFF;
    }
    else if (strncmp(text, "HEARTBEAT ", 10) == 0 &&
             parse_unsigned(text + 10, heartbeat) == 0)
    {
        *request = UART_REQUEST_HEARTBEAT;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static int validate_reply(UartRequest request, uint32_t heartbeat, const char *reply)
{
    uint32_t value;

    switch (request)
    {
        case UART_REQUEST_PING:
            return strcmp(reply, "PONG") == 0;
        case UART_REQUEST_STATUS:
            return strlen(reply) >= 24 &&
                   strncmp(reply, "STATUS LED=", 11) == 0 &&
                   (reply[11] == '0' || reply[11] == '1') &&
                   strncmp(reply + 12, " UPTIME_MS=", 11) == 0 &&
                   parse_unsigned(reply + 23, &value) == 0;
        case UART_REQUEST_LED_ON:
            return strcmp(reply, "OK LED=1") == 0;
        case UART_REQUEST_LED_OFF:
            return strcmp(reply, "OK LED=0") == 0;
        case UART_REQUEST_HEARTBEAT:
            return strncmp(reply, "OK HEARTBEAT=", 13) == 0 &&
                   parse_unsigned(reply + 13, &value) == 0 && value == heartbeat;
        default:
            return 0;
    }
}

static int64_t monotonic_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
    {
        return -1;
    }

    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int remaining_ms(int64_t deadline)
{
    int64_t now = monotonic_ms();

    if (now < 0)
    {
        return -1;
    }

    int64_t remaining = deadline - now;

    if (remaining <= 0)
    {
        errno = ETIMEDOUT;
        return -1;
    }

    return remaining > INT_MAX ? INT_MAX : (int)remaining;
}

static int protocol_result(Device *device, int result)
{
    if (device)
    {
        device->last_errno = result < 0 ? errno : 0;
    }

    return result;
}

int uart_protocol_request(Device *device, const char *request,
                          char *reply, size_t size, int timeout_ms)
{
    if (!device || device->config.type != DEVICE_UART || !request ||
        !reply || size == 0 || timeout_ms < 0)
    {
        errno = EINVAL;
        return protocol_result(device, -1);
    }

    if (!device->config.enabled)
    {
        errno = EACCES;
        return protocol_result(device, -1);
    }

    if (!device->opened)
    {
        errno = EBADF;
        return protocol_result(device, -1);
    }

    size_t length = strnlen(request, UART_PROTOCOL_LINE_SIZE);

    if (length >= UART_PROTOCOL_LINE_SIZE)
    {
        errno = EMSGSIZE;
        return protocol_result(device, -1);
    }

    UartRequest request_type;
    uint32_t heartbeat = 0;

    if (parse_request(request, &request_type, &heartbeat) < 0)
    {
        return protocol_result(device, -1);
    }

    char packet[UART_PROTOCOL_LINE_SIZE + 1];
    memcpy(packet, request, length);
    packet[length] = '\n';
    reply[0] = '\0';

    if (timeout_ms == 0)
    {
        errno = ETIMEDOUT;
        return protocol_result(device, -1);
    }

    int64_t now = monotonic_ms();

    if (now < 0)
    {
        return protocol_result(device, -1);
    }

    int64_t deadline = now + timeout_ms;

    if (device_control(device, DEVICE_CONTROL_UART_FLUSH, NULL) < 0)
    {
        return protocol_result(device, -1);
    }

    DeviceWriteRequest write_request;
    write_request.data = packet;
    write_request.size = length + 1;
    write_request.timeout_ms = remaining_ms(deadline);

    if (write_request.timeout_ms < 0 ||
        device_control(device, DEVICE_CONTROL_UART_WRITE_ALL, &write_request) < 0)
    {
        return protocol_result(device, -1);
    }

    char line[UART_PROTOCOL_LINE_SIZE];
    size_t received = 0;
    int carriage_return = 0;

    for (;;)
    {
        int timeout = remaining_ms(deadline);

        if (timeout < 0)
        {
            return protocol_result(device, -1);
        }

        int ready = device_control(device, DEVICE_CONTROL_UART_WAIT, &timeout);

        if (ready < 0)
        {
            return protocol_result(device, -1);
        }

        if (ready == 0)
        {
            errno = ETIMEDOUT;
            return protocol_result(device, -1);
        }

        unsigned char byte;
        ssize_t result = device_read(device, &byte, sizeof(byte));

        if (result < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }

            return protocol_result(device, -1);
        }

        if (result == 0)
        {
            errno = EIO;
            device->state = DEVICE_ERROR;
            return protocol_result(device, -1);
        }

        if (byte == '\n')
        {
            line[received] = '\0';
            break;
        }

        if (carriage_return)
        {
            errno = EPROTO;
            return protocol_result(device, -1);
        }

        if (byte == '\r')
        {
            carriage_return = 1;
            continue;
        }

        if (byte < 0x20 || byte > 0x7e)
        {
            errno = EPROTO;
            return protocol_result(device, -1);
        }

        if (received >= sizeof(line) - 1)
        {
            errno = EMSGSIZE;
            return protocol_result(device, -1);
        }

        line[received++] = (char)byte;
    }

    if (remaining_ms(deadline) < 0)
    {
        return protocol_result(device, -1);
    }

    if (!validate_reply(request_type, heartbeat, line))
    {
        errno = EPROTO;
        return protocol_result(device, -1);
    }

    if (received >= size)
    {
        errno = EMSGSIZE;
        return protocol_result(device, -1);
    }

    memcpy(reply, line, received + 1);
    return protocol_result(device, 0);
}
