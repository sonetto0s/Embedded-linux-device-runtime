#ifndef STM32_UART_PROTOCOL_H
#define STM32_UART_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define STM32_UART_PROTOCOL_LINE_SIZE 128

typedef struct
{
    int (*set_led)(void *context, int enabled);
    int (*get_led)(void *context);
    uint32_t (*uptime_ms)(void *context);
    int (*send)(void *context, const char *data, size_t size);
    void *context;
} Stm32UartCallbacks;

typedef struct
{
    Stm32UartCallbacks callbacks;
    char line[STM32_UART_PROTOCOL_LINE_SIZE];
    size_t length;
    int carriage_return;
    int discarding;
    int line_too_long;
} Stm32UartProtocol;

int stm32_uart_protocol_init(Stm32UartProtocol *protocol,
                             const Stm32UartCallbacks *callbacks);
int stm32_uart_protocol_handle(const Stm32UartCallbacks *callbacks,
                               const char *request, char *reply, size_t size);
int stm32_uart_protocol_feed(Stm32UartProtocol *protocol, unsigned char byte);

#endif
