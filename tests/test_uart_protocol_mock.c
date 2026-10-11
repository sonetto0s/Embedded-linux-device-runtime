#include "device.h"
#include "uart_protocol.h"
#include "test_framework.h"
#include "../examples/stm32_uart_protocol/stm32_uart_protocol.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct
{
    int led;
    uint32_t uptime;
    char incoming[512];
    size_t length;
    size_t position;
    const char *forced_reply;
    int crlf;
    int suppress_reply;
    int transport_error;
    int delay_ms;
    int open_count;
    int flush_count;
    int write_timeout;
    int first_wait_timeout;
} MockUart;

static int mock_set_led(void *context, int enabled)
{
    ((MockUart *)context)->led = enabled;
    return 0;
}

static int mock_get_led(void *context)
{
    return ((MockUart *)context)->led;
}

static uint32_t mock_uptime(void *context)
{
    return ((MockUart *)context)->uptime;
}

static void mock_delay(int milliseconds)
{
#ifdef _WIN32
    Sleep((DWORD)milliseconds);
#else
    struct timespec request;
    request.tv_sec = milliseconds / 1000;
    request.tv_nsec = (long)(milliseconds % 1000) * 1000000L;

    while (nanosleep(&request, &request) < 0 && errno == EINTR)
    {
    }
#endif
}

static int mock_open(Device *device)
{
    ((MockUart *)device->context)->open_count++;
    return 0;
}

static ssize_t mock_read(Device *device, void *buffer, size_t size)
{
    MockUart *uart = device->context;

    if (uart->position == uart->length)
    {
        errno = EAGAIN;
        return -1;
    }

    size_t available = uart->length - uart->position;

    if (size > available)
    {
        size = available;
    }

    if (size > 1)
    {
        size = 1;
    }

    memcpy(buffer, uart->incoming + uart->position, size);
    uart->position += size;
    return (ssize_t)size;
}

static int mock_write_request(MockUart *uart, const DeviceWriteRequest *request)
{
    if (!request || !request->data || request->size == 0 ||
        request->size > UART_PROTOCOL_LINE_SIZE)
    {
        errno = EINVAL;
        return -1;
    }

    const char *data = request->data;

    if (data[request->size - 1] != '\n')
    {
        errno = EPROTO;
        return -1;
    }

    uart->write_timeout = request->timeout_ms;

    if (uart->delay_ms > 0)
    {
        mock_delay(uart->delay_ms);
    }

    if (uart->transport_error)
    {
        errno = EIO;
        return -1;
    }

    if (uart->suppress_reply)
    {
        return 0;
    }

    char text[UART_PROTOCOL_LINE_SIZE];
    memcpy(text, data, request->size - 1);
    text[request->size - 1] = '\0';

    Stm32UartCallbacks callbacks =
    {
        mock_set_led, mock_get_led, mock_uptime, NULL, uart
    };

    char response[STM32_UART_PROTOCOL_LINE_SIZE];
    const char *reply = uart->forced_reply;

    if (!reply)
    {
        if (stm32_uart_protocol_handle(&callbacks, text, response, sizeof(response)) < 0)
        {
            errno = EIO;
            return -1;
        }

        reply = response;
    }

    size_t length = strlen(reply);

    if (length >= sizeof(uart->incoming) - 1)
    {
        errno = EMSGSIZE;
        return -1;
    }

    memcpy(uart->incoming, reply, length);

    if (uart->crlf && length > 0 && reply[length - 1] == '\n')
    {
        uart->incoming[length - 1] = '\r';
        uart->incoming[length++] = '\n';
    }

    uart->length = length;
    return 0;
}

static int mock_control(Device *device, DeviceControl request, void *argument)
{
    MockUart *uart = device->context;

    if (request == DEVICE_CONTROL_UART_FLUSH)
    {
        uart->position = 0;
        uart->length = 0;
        uart->first_wait_timeout = 0;
        uart->flush_count++;
        return 0;
    }

    if (request == DEVICE_CONTROL_UART_WRITE_ALL)
    {
        return mock_write_request(uart, argument);
    }

    if (request == DEVICE_CONTROL_UART_WAIT)
    {
        if (!argument)
        {
            errno = EINVAL;
            return -1;
        }

        if (!uart->first_wait_timeout)
        {
            uart->first_wait_timeout = *(int *)argument;
        }

        return uart->position < uart->length;
    }

    errno = ENOTSUP;
    return -1;
}

static int mock_close(Device *device)
{
    (void)device;
    return 0;
}

static const DeviceOps mock_ops =
{
    mock_open, mock_read, NULL, mock_control, mock_close, NULL
};

static void mock_init(Device *device, MockUart *uart)
{
    memset(uart, 0, sizeof(*uart));
    uart->uptime = 1234;

    DeviceConfig config;
    memset(&config, 0, sizeof(config));
    strcpy(config.name, "stm32_mock");
    strcpy(config.path, "/mock/uart0");
    config.type = DEVICE_UART;
    config.enabled = 1;
    config.baudrate = 115200;

    device_init(device, &config, &mock_ops, uart);
}

void test_uart_protocol_mock_roundtrip(void)
{
    Device device;
    MockUart uart;
    char reply[UART_PROTOCOL_LINE_SIZE];

    mock_init(&device, &uart);
    TEST_ASSERT_EQ(device_open(&device), 0);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_STR_EQ(reply, "PONG");
    TEST_ASSERT_EQ(uart_protocol_request(&device, "STATUS", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_STR_EQ(reply, "STATUS LED=0 UPTIME_MS=1234");
    TEST_ASSERT_EQ(uart_protocol_request(&device, "LED ON", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_STR_EQ(reply, "OK LED=1");
    TEST_ASSERT_EQ(uart.led, 1);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "STATUS", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_STR_EQ(reply, "STATUS LED=1 UPTIME_MS=1234");
    TEST_ASSERT_EQ(uart_protocol_request(&device, "LED OFF", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_STR_EQ(reply, "OK LED=0");
    TEST_ASSERT_EQ(uart.led, 0);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "HEARTBEAT 0", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_STR_EQ(reply, "OK HEARTBEAT=0");
    TEST_ASSERT_EQ(uart_protocol_request(&device, "HEARTBEAT 4294967295", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_STR_EQ(reply, "OK HEARTBEAT=4294967295");

    uart.crlf = 1;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_STR_EQ(reply, "PONG");
    TEST_ASSERT_EQ(device.last_errno, 0);
    TEST_ASSERT_EQ(device.state, DEVICE_OPEN);
    TEST_ASSERT_EQ(uart.open_count, 1);
    TEST_ASSERT_EQ(device_close(&device), 0);
}

void test_uart_protocol_mock_validation(void)
{
    Device device;
    MockUart uart;
    char reply[UART_PROTOCOL_LINE_SIZE];

    mock_init(&device, &uart);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EBADF);
    TEST_ASSERT_EQ(uart.open_count, 0);

    device.config.enabled = 0;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EACCES);

    device.config.enabled = 1;
    TEST_ASSERT_EQ(device_open(&device), 0);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING\n", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "HEARTBEAT 4294967296", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "HEARTBEAT -1", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), -1), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, 0, 500), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 0), -1);
    TEST_ASSERT_EQ(errno, ETIMEDOUT);
    TEST_ASSERT_EQ(uart.flush_count, 0);

    char request[UART_PROTOCOL_LINE_SIZE + 1];
    memset(request, 'P', sizeof(request) - 1);
    request[sizeof(request) - 1] = '\0';
    TEST_ASSERT_EQ(uart_protocol_request(&device, request, reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EMSGSIZE);
    TEST_ASSERT_EQ(device_close(&device), 0);
}

void test_uart_protocol_mock_failures(void)
{
    Device device;
    MockUart uart;
    char reply[UART_PROTOCOL_LINE_SIZE];

    mock_init(&device, &uart);
    TEST_ASSERT_EQ(device_open(&device), 0);
    uart.forced_reply = "NOPE\n";
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EPROTO);
    TEST_ASSERT_EQ(device.last_errno, EPROTO);
    TEST_ASSERT_EQ(device.state, DEVICE_OPEN);

    uart.forced_reply = "PO\rNG\n";
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EPROTO);

    uart.forced_reply = "STATUS LED=2 UPTIME_MS=1234\n";
    TEST_ASSERT_EQ(uart_protocol_request(&device, "STATUS", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EPROTO);

    uart.forced_reply = "STATUS LED=0 UPTIME_MS=4294967296\n";
    TEST_ASSERT_EQ(uart_protocol_request(&device, "STATUS", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EPROTO);

    uart.forced_reply = "OK HEARTBEAT=43\n";
    TEST_ASSERT_EQ(uart_protocol_request(&device, "HEARTBEAT 42", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EPROTO);

    uart.forced_reply = NULL;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, 4, 500), -1);
    TEST_ASSERT_EQ(errno, EMSGSIZE);

    char overlong[UART_PROTOCOL_LINE_SIZE + 3];
    memset(overlong, 'P', sizeof(overlong) - 2);
    overlong[sizeof(overlong) - 2] = '\n';
    overlong[sizeof(overlong) - 1] = '\0';
    uart.forced_reply = overlong;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EMSGSIZE);

    uart.forced_reply = NULL;
    uart.suppress_reply = 1;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, ETIMEDOUT);
    TEST_ASSERT_EQ(device.last_errno, ETIMEDOUT);
    TEST_ASSERT_EQ(device.state, DEVICE_OPEN);

    uart.suppress_reply = 0;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), 0);
    TEST_ASSERT_EQ(device.last_errno, 0);

    uart.transport_error = 1;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), -1);
    TEST_ASSERT_EQ(errno, EIO);
    TEST_ASSERT_EQ(device.last_errno, EIO);
    TEST_ASSERT_EQ(device.state, DEVICE_ERROR);
    TEST_ASSERT_EQ(device_close(&device), 0);
}

void test_uart_protocol_mock_deadline(void)
{
    Device device;
    MockUart uart;
    char reply[UART_PROTOCOL_LINE_SIZE];

    mock_init(&device, &uart);
    TEST_ASSERT_EQ(device_open(&device), 0);
    uart.delay_ms = 40;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 500), 0);
    TEST_ASSERT(uart.write_timeout > 0 && uart.write_timeout <= 500);
    TEST_ASSERT(uart.first_wait_timeout > 0);
    TEST_ASSERT(uart.first_wait_timeout <= uart.write_timeout - 30);

    uart.delay_ms = 40;
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 10), -1);
    TEST_ASSERT_EQ(errno, ETIMEDOUT);
    TEST_ASSERT_EQ(uart.first_wait_timeout, 0);
    TEST_ASSERT_EQ(device.state, DEVICE_OPEN);
    TEST_ASSERT_EQ(device_close(&device), 0);
}
