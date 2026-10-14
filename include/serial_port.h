#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include <stddef.h>
#include <sys/types.h>

typedef struct
{
    int fd;
} SerialPort;

void serial_port_init(SerialPort *port);
int serial_port_open(SerialPort *port, const char *device_path, int baudrate, int nonblocking);
ssize_t serial_port_read(SerialPort *port, void *buffer, size_t size);
ssize_t serial_port_write(SerialPort *port, const void *buffer, size_t size);
int serial_port_write_all(SerialPort *port, const void *buffer, size_t size, int timeout_ms);
int serial_port_wait_readable(SerialPort *port, int timeout_ms);
int serial_port_flush_input(SerialPort *port);
void serial_port_close(SerialPort *port);

#endif
