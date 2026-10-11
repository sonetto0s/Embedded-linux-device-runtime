#define _XOPEN_SOURCE 600
#include "device_backend.h"
#include "device_config.h"
#include "uart_protocol.h"
#include "test_framework.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void protocol_sleep_ms(int ms)
{
    struct timespec delay = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
    {
    }
}

static long long protocol_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
        return -1;
    }
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int protocol_start_device(Device *device)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0)
    {
        return -1;
    }
    if (grantpt(master) < 0 || unlockpt(master) < 0)
    {
        close(master);
        return -1;
    }
    const char *path = ptsname(master);
    DeviceConfig config;
    device_config_init(&config);
    config.type = DEVICE_UART;
    strcpy(config.name, "stm32_test");
    if (!path || strlen(path) >= sizeof(config.path))
    {
        close(master);
        return -1;
    }
    strcpy(config.path, path);
    if (device_backend_init(device, &config) < 0)
    {
        close(master);
        return -1;
    }
    if (device_open(device) < 0)
    {
        device_destroy(device);
        close(master);
        return -1;
    }
    return master;
}

static void protocol_peer(int master, const char *expected, const char *reply, int byte_delay_ms,
                          int hold_ms)
{
    char request[256];
    size_t used = 0;
    while (used + 1 < sizeof(request))
    {
        struct pollfd pfd = {.fd = master, .events = POLLIN};
        if (poll(&pfd, 1, 1500) != 1)
        {
            _exit(2);
        }
        ssize_t n = read(master, request + used, 1);
        if (n != 1)
        {
            _exit(3);
        }
        if (request[used++] == '\n')
        {
            break;
        }
    }
    request[used] = '\0';
    char expected_line[256];
    snprintf(expected_line, sizeof(expected_line), "%s\n", expected);
    if (strcmp(request, expected_line) != 0)
    {
        _exit(4);
    }
    if (reply)
    {
        size_t length = strlen(reply);
        if (byte_delay_ms == 0)
        {
            if (write(master, reply, length) != (ssize_t)length)
            {
                _exit(5);
            }
        }
        else
        {
            for (size_t i = 0; i < length; i++)
            {
                if (write(master, reply + i, 1) != 1)
                {
                    break;
                }
                protocol_sleep_ms(byte_delay_ms);
            }
        }
    }
    protocol_sleep_ms(hold_ms);
    close(master);
    _exit(0);
}

static void protocol_exchange(const char *request, const char *peer_reply,
                              const char *expected_reply, int expected_errno, int delay_ms,
                              int hold_ms, int timeout_ms, size_t reply_size)
{
    Device device;
    int master = protocol_start_device(&device);
    TEST_ASSERT(master >= 0);
    if (master < 0)
    {
        return;
    }
    pid_t peer = fork();
    TEST_ASSERT(peer >= 0);
    if (peer < 0)
    {
        device_destroy(&device);
        close(master);
        return;
    }
    if (peer == 0)
    {
        /* Do not retain the slave in the fake STM32 process. */
        device_destroy(&device);
        if (peer_reply && hold_ms < 100)
        {
            hold_ms = 100;
        }
        protocol_peer(master, request, peer_reply, delay_ms, hold_ms);
    }
    close(master);
    char reply[256];
    memset(reply, 0xAA, sizeof(reply));
    long long start = protocol_now_ms();
    errno = 0;
    int ret = uart_protocol_request(&device, request, reply, reply_size, timeout_ms);
    int actual_errno = errno;
    long long elapsed = protocol_now_ms() - start;
    TEST_ASSERT_EQ(ret, expected_errno ? -1 : 0);
    if (expected_errno)
    {
        TEST_ASSERT_EQ(actual_errno, expected_errno);
    }
    else
    {
        TEST_ASSERT_STR_EQ(reply, expected_reply);
    }
    if (expected_errno == ETIMEDOUT)
    {
        TEST_ASSERT(elapsed < 1000);
        TEST_ASSERT_EQ(device.state, DEVICE_OPEN);
        TEST_ASSERT_EQ(device.opened, 1);
    }
    if (expected_errno == EPROTO || expected_errno == EMSGSIZE)
    {
        TEST_ASSERT_EQ(device.state, DEVICE_OPEN);
    }
    if (expected_errno == EIO)
    {
        TEST_ASSERT(elapsed < 1000);
        TEST_ASSERT_EQ(device.state, DEVICE_ERROR);
        TEST_ASSERT_EQ(device.last_errno, EIO);
        TEST_ASSERT_EQ(device.opened, 1);
    }
    device_destroy(&device);
    int status = 0;
    pid_t waited;
    long long deadline = protocol_now_ms() + 2000;
    do
    {
        waited = waitpid(peer, &status, WNOHANG);
        if (waited == 0)
        {
            protocol_sleep_ms(10);
        }
    } while ((waited == 0 || (waited < 0 && errno == EINTR)) && protocol_now_ms() < deadline);
    if (waited == 0)
    {
        kill(peer, SIGKILL);
        waitpid(peer, &status, 0);
    }
    TEST_ASSERT_EQ(waited, peer);
    TEST_ASSERT(WIFEXITED(status));
    if (WIFEXITED(status))
    {
        TEST_ASSERT_EQ(WEXITSTATUS(status), 0);
    }
}

void test_uart_protocol_commands_pty(void)
{
    protocol_exchange("PING", "PONG\r\n", "PONG", 0, 5, 0, 1000, 256);
    protocol_exchange("STATUS", "STATUS LED=1 UPTIME_MS=4294967295\n",
                      "STATUS LED=1 UPTIME_MS=4294967295", 0, 1, 0, 1000, 256);
    protocol_exchange("LED ON", "OK LED=1\n", "OK LED=1", 0, 2, 0, 1000, 256);
    protocol_exchange("LED OFF", "OK LED=0\r\n", "OK LED=0", 0, 2, 0, 1000, 256);
    protocol_exchange("HEARTBEAT 4294967295", "OK HEARTBEAT=4294967295\n",
                      "OK HEARTBEAT=4294967295", 0, 1, 0, 1000, 256);
}

void test_uart_protocol_reply_validation_pty(void)
{
    /* A peer that closes after the request must fail without spinning until timeout. */
    protocol_exchange("PING", NULL, NULL, EIO, 0, 0, 1000, 256);
    protocol_exchange("PING", "OK LED=1\n", NULL, EPROTO, 0, 20, 1000, 256);
    protocol_exchange("LED OFF", "OK LED=1\n", NULL, EPROTO, 0, 20, 1000, 256);
    protocol_exchange("STATUS", "STATUS LED=2 UPTIME_MS=1\n", NULL, EPROTO, 0, 20, 1000, 256);
    protocol_exchange("STATUS", "STATUS LED=1 UPTIME_MS=4294967296\n", NULL, EPROTO, 0, 20, 1000,
                      256);
    protocol_exchange("HEARTBEAT 7", "OK HEARTBEAT=8\n", NULL, EPROTO, 0, 20, 1000, 256);
    protocol_exchange("PING", "PONG\n", NULL, EMSGSIZE, 0, 20, 1000, 4);
    char overlong[UART_PROTOCOL_LINE_SIZE + 4];
    memset(overlong, 'x', sizeof(overlong));
    overlong[sizeof(overlong) - 2] = '\n';
    overlong[sizeof(overlong) - 1] = '\0';
    protocol_exchange("PING", overlong, NULL, EMSGSIZE, 0, 20, 1000, 256);
}

void test_uart_protocol_deadline_pty(void)
{
    protocol_exchange("PING", NULL, NULL, ETIMEDOUT, 0, 200, 60, 256);
    protocol_exchange("PING", "PONG", NULL, ETIMEDOUT, 0, 200, 60, 256);
    /* Receiving partial bytes must not renew the transaction deadline. */
    protocol_exchange("PING", "PONG\n", NULL, ETIMEDOUT, 30, 100, 60, 256);
}

void test_uart_protocol_invalid_requests(void)
{
    Device device;
    int master = protocol_start_device(&device);
    TEST_ASSERT(master >= 0);
    if (master < 0)
    {
        return;
    }
    char reply[UART_PROTOCOL_LINE_SIZE];
    const char *bad[] = {
        "", "PING\n", "PING\r", "REBOOT", "LED TOGGLE", "HEARTBEAT -1", "HEARTBEAT 4294967296"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        TEST_ASSERT_EQ(uart_protocol_request(&device, bad[i], reply, sizeof(reply), 100), -1);
        TEST_ASSERT_EQ(errno, EINVAL);
    }
    struct pollfd pfd = {.fd = master, .events = POLLIN};
    TEST_ASSERT_EQ(poll(&pfd, 1, 0), 0);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), 0), -1);
    TEST_ASSERT_EQ(errno, ETIMEDOUT);
    TEST_ASSERT_EQ(uart_protocol_request(&device, "PING", reply, sizeof(reply), -1), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(uart_protocol_request(NULL, "PING", reply, sizeof(reply), 100), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    device_destroy(&device);
    close(master);
}
