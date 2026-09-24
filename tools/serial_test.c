#include "serial_port.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERIAL_TIMEOUT_MS 5000
#define SERIAL_BUFFER_SIZE 1024

static int parse_baudrate(const char *text, int *baudrate)
{
    if (!text || !baudrate)
    {
        return -1;
    }

    errno = 0;
    char *end = NULL;
    
    long value = strtol(text, &end, 10);

    if (errno != 0 || !end || *end != '\0' || value <= 0 || value > 10000000)
    {
        return -1;
    }

    *baudrate = (int)value;
    return 0;
}

static int run_send(SerialPort *port, const char *message)
{
    size_t length = strlen(message);

    if (serial_port_write_all(port, message, length, SERIAL_TIMEOUT_MS) < 0)
    {
        perror("serial_port_write_all");
        return -1;
    }

    printf("TX (%zu bytes): %s\n", length, message);
    return 0;
}

static int run_receive(SerialPort *port)
{
    unsigned char buffer[SERIAL_BUFFER_SIZE];

    printf("Waiting for serial data. Press Ctrl+C to stop.\n");

    for (;;)
    {
        int ready = serial_port_wait_readable(port, 1000);

        if (ready < 0)
        {
            perror("serial_port_wait_readable");
            return -1;
        }

        if (ready == 0)
        {
            continue;
        }

        ssize_t received = serial_port_read(port, buffer, sizeof(buffer));

        if (received > 0)
        {
            printf("RX (%zd bytes): ", received);
            fwrite(buffer, 1, (size_t)received, stdout);
            printf("\n");
            fflush(stdout);
            continue;
        }

        if (received == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        {
            continue;
        }

        perror("serial_port_read");
        return -1;
    }
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s <device> <baudrate> send <message>\n", argv[0]);
        fprintf(stderr, "  %s <device> <baudrate> recv\n", argv[0]);
        return EXIT_FAILURE;
    }

    int baudrate;

    if (parse_baudrate(argv[2], &baudrate) < 0)
    {
        fprintf(stderr, "Invalid baudrate: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    SerialPort port;
    serial_port_init(&port);

    if (serial_port_open(&port, argv[1], baudrate, 1) < 0)
    {
        fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Device : %s\n", argv[1]);
    printf("Baud   : %d\n", baudrate);
    printf("Mode   : 8N1 raw nonblocking\n");

    int result = -1;

    if (strcmp(argv[3], "send") == 0)
    {
        if (argc != 5)
        {
            fprintf(stderr, "send requires exactly one message argument\n");
        }
        else
        {
            result = run_send(&port, argv[4]);
        }
    }
    else if (strcmp(argv[3], "recv") == 0)
    {
        if (argc != 4)
        {
            fprintf(stderr, "recv takes no message argument\n");
        }
        else
        {
            result = run_receive(&port);
        }
    }
    else
    {
        fprintf(stderr, "Unknown mode: %s\n", argv[3]);
    }

    serial_port_close(&port);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
