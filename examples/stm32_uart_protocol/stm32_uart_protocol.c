#include "stm32_uart_protocol.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

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

static int copy_reply(char *reply, size_t size, const char *text)
{
    size_t length = strlen(text);

    if (length >= size)
    {
        return -1;
    }

    memcpy(reply, text, length + 1);
    return 0;
}

int stm32_uart_protocol_handle(const Stm32UartCallbacks *callbacks,
                               const char *request, char *reply, size_t size)
{
    if (!callbacks || !request || !reply || size == 0 ||
        !callbacks->set_led || !callbacks->get_led || !callbacks->uptime_ms)
    {
        return -1;
    }

    reply[0] = '\0';

    size_t length = 0;

    while (length < STM32_UART_PROTOCOL_LINE_SIZE && request[length])
    {
        length++;
    }

    if (length >= STM32_UART_PROTOCOL_LINE_SIZE)
    {
        return copy_reply(reply, size, "ERR LINE_TOO_LONG\n");
    }

    if (strcmp(request, "PING") == 0)
    {
        return copy_reply(reply, size, "PONG\n");
    }

    if (strcmp(request, "STATUS") == 0)
    {
        int led = callbacks->get_led(callbacks->context);

        if (led != 0 && led != 1)
        {
            return copy_reply(reply, size, "ERR DEVICE\n");
        }

        int written = snprintf(reply, size, "STATUS LED=%d UPTIME_MS=%" PRIu32 "\n",
                               led, callbacks->uptime_ms(callbacks->context));

        return written >= 0 && (size_t)written < size ? 0 : -1;
    }

    if (strcmp(request, "LED ON") == 0 || strcmp(request, "LED OFF") == 0)
    {
        int enabled = strcmp(request, "LED ON") == 0;

        if (callbacks->set_led(callbacks->context, enabled) != 0 ||
            callbacks->get_led(callbacks->context) != enabled)
        {
            return copy_reply(reply, size, "ERR DEVICE\n");
        }

        return copy_reply(reply, size, enabled ? "OK LED=1\n" : "OK LED=0\n");
    }

    if (strncmp(request, "HEARTBEAT ", 10) == 0)
    {
        uint32_t sequence;

        if (parse_unsigned(request + 10, &sequence) < 0)
        {
            return copy_reply(reply, size, "ERR ARGUMENT\n");
        }

        int written = snprintf(reply, size, "OK HEARTBEAT=%" PRIu32 "\n", sequence);

        return written >= 0 && (size_t)written < size ? 0 : -1;
    }

    return copy_reply(reply, size, "ERR COMMAND\n");
}

int stm32_uart_protocol_init(Stm32UartProtocol *protocol,
                             const Stm32UartCallbacks *callbacks)
{
    if (!protocol || !callbacks || !callbacks->set_led || !callbacks->get_led ||
        !callbacks->uptime_ms || !callbacks->send)
    {
        return -1;
    }

    memset(protocol, 0, sizeof(*protocol));
    protocol->callbacks = *callbacks;
    return 0;
}

int stm32_uart_protocol_feed(Stm32UartProtocol *protocol, unsigned char byte)
{
    if (!protocol || !protocol->callbacks.send)
    {
        return -1;
    }

    if (byte == '\n')
    {
        char reply[STM32_UART_PROTOCOL_LINE_SIZE];
        int result;

        protocol->line[protocol->length] = '\0';

        if (protocol->discarding)
        {
            result = copy_reply(reply, sizeof(reply), protocol->line_too_long ?
                                "ERR LINE_TOO_LONG\n" : "ERR COMMAND\n");
        }
        else
        {
            result = stm32_uart_protocol_handle(&protocol->callbacks,
                                                protocol->line, reply, sizeof(reply));
        }

        protocol->length = 0;
        protocol->carriage_return = 0;
        protocol->discarding = 0;
        protocol->line_too_long = 0;

        if (result < 0)
        {
            return -1;
        }

        return protocol->callbacks.send(protocol->callbacks.context, reply, strlen(reply));
    }

    if (protocol->discarding)
    {
        return 0;
    }

    if (protocol->carriage_return)
    {
        protocol->discarding = 1;
        return 0;
    }

    if (byte == '\r')
    {
        protocol->carriage_return = 1;
        return 0;
    }

    if (byte < 0x20 || byte > 0x7e)
    {
        protocol->discarding = 1;
        return 0;
    }

    if (protocol->length >= sizeof(protocol->line) - 1)
    {
        protocol->discarding = 1;
        protocol->line_too_long = 1;
        return 0;
    }

    protocol->line[protocol->length++] = (char)byte;
    return 0;
}
