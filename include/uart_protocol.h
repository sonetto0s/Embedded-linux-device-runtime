#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include "device.h"

#define UART_PROTOCOL_LINE_SIZE 128

int uart_protocol_request(Device *device, const char *request,
                          char *reply, size_t size, int timeout_ms);

#endif
