#define _XOPEN_SOURCE 600
#include "serial_port.h"
#include "test_framework.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static int serial_pty(char *path, size_t size)
{
    int fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        return -1;
    }
    if (grantpt(fd) < 0 || unlockpt(fd) < 0)
    {
        close(fd);
        return -1;
    }
    const char *name = ptsname(fd);
    if (!name || strlen(name) >= size)
    {
        close(fd);
        return -1;
    }
    strcpy(path, name);
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

static long long serial_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
        return -1;
    }
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void test_serial_port_raw_binary_pty(void)
{
    char path[256];
    int master = serial_pty(path, sizeof(path));
    TEST_ASSERT(master >= 0);
    if (master < 0)
    {
        return;
    }
    SerialPort port;
    serial_port_init(&port);
    int ret = serial_port_open(&port, path, 115200, 1);
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        close(master);
        return;
    }
    struct termios modes;
    TEST_ASSERT_EQ(tcgetattr(port.fd, &modes), 0);
    TEST_ASSERT((modes.c_lflag & (ECHO | ICANON | ISIG)) == 0);
    TEST_ASSERT((modes.c_oflag & OPOST) == 0);
    TEST_ASSERT((modes.c_iflag & (ICRNL | IXON | IXOFF)) == 0);
    TEST_ASSERT_EQ(modes.c_cflag & CSIZE, CS8);
    TEST_ASSERT((modes.c_cflag & (PARENB | CSTOPB)) == 0);
    TEST_ASSERT_EQ(cfgetispeed(&modes), B115200);
    TEST_ASSERT((fcntl(port.fd, F_GETFL) & O_NONBLOCK) != 0);
    TEST_ASSERT((fcntl(port.fd, F_GETFD) & FD_CLOEXEC) != 0);
    const unsigned char bytes[] = {0x00, '\r', '\n', 3, 0x80, 0xff};
    unsigned char received[sizeof(bytes)] = {0};
    TEST_ASSERT_EQ(write(master, bytes, sizeof(bytes)), sizeof(bytes));
    TEST_ASSERT_EQ(serial_port_wait_readable(&port, 1000), 1);
    TEST_ASSERT_EQ(serial_port_read(&port, received, sizeof(received)), sizeof(bytes));
    TEST_ASSERT_EQ(memcmp(received, bytes, sizeof(bytes)), 0);
    TEST_ASSERT_EQ(serial_port_write_all(&port, bytes, sizeof(bytes), 1000), 0);
    struct pollfd pfd = {.fd = master, .events = POLLIN};
    TEST_ASSERT_EQ(poll(&pfd, 1, 1000), 1);
    memset(received, 0, sizeof(received));
    TEST_ASSERT_EQ(read(master, received, sizeof(received)), sizeof(bytes));
    TEST_ASSERT_EQ(memcmp(received, bytes, sizeof(bytes)), 0);
    serial_port_close(&port);
    close(master);
}

void test_serial_port_wait_flush_pty(void)
{
    char path[256];
    int master = serial_pty(path, sizeof(path));
    TEST_ASSERT(master >= 0);
    if (master < 0)
    {
        return;
    }
    SerialPort port;
    serial_port_init(&port);
    int ret = serial_port_open(&port, path, 9600, 1);
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        close(master);
        return;
    }
    long long start = serial_now_ms();
    TEST_ASSERT_EQ(serial_port_wait_readable(&port, 40), 0);
    long long elapsed = serial_now_ms() - start;
    TEST_ASSERT(elapsed >= 20 && elapsed < 1500);
    TEST_ASSERT_EQ(write(master, "stale", 5), 5);
    TEST_ASSERT_EQ(serial_port_wait_readable(&port, 1000), 1);
    TEST_ASSERT_EQ(serial_port_flush_input(&port), 0);
    TEST_ASSERT_EQ(serial_port_wait_readable(&port, 0), 0);
    TEST_ASSERT_EQ(serial_port_wait_readable(&port, -1), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    serial_port_close(&port);
    close(master);
}

void test_serial_port_lifecycle_pty(void)
{
    char path[256];
    int master = serial_pty(path, sizeof(path));
    TEST_ASSERT(master >= 0);
    if (master < 0)
    {
        return;
    }
    SerialPort port;
    serial_port_init(&port);
    TEST_ASSERT_EQ(serial_port_open(&port, path, 12345, 1), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(port.fd, -1);
    for (int round = 0; round < 8; round++)
    {
        int ret = serial_port_open(&port, path, 115200, 1);
        TEST_ASSERT_EQ(ret, 0);
        if (ret < 0)
        {
            break;
        }
        int owned_fd = port.fd;
        TEST_ASSERT_EQ(serial_port_open(&port, path, 115200, 1), -1);
        TEST_ASSERT_EQ(errno, EBUSY);
        TEST_ASSERT_EQ(port.fd, owned_fd);
        serial_port_close(&port);
        TEST_ASSERT_EQ(port.fd, -1);
        errno = 0;
        TEST_ASSERT_EQ(fcntl(owned_fd, F_GETFD), -1);
        TEST_ASSERT_EQ(errno, EBADF);
        serial_port_close(&port);
    }
    serial_port_close(&port);
    close(master);
}

void test_serial_port_write_deadline_pty(void)
{
    char path[256];
    int master = serial_pty(path, sizeof(path));
    TEST_ASSERT(master >= 0);
    if (master < 0)
    {
        return;
    }
    SerialPort port;
    serial_port_init(&port);
    int ret = serial_port_open(&port, path, 115200, 1);
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        close(master);
        return;
    }
    size_t size = 1024 * 1024;
    unsigned char *data = malloc(size);
    TEST_ASSERT_NOT_NULL(data);
    if (data)
    {
        memset(data, 0x55, size);
        long long start = serial_now_ms();
        TEST_ASSERT_EQ(serial_port_write_all(&port, data, size, 50), -1);
        TEST_ASSERT_EQ(errno, ETIMEDOUT);
        TEST_ASSERT(serial_now_ms() - start < 1500);
        free(data);
    }
    serial_port_close(&port);
    close(master);
}

void test_serial_port_peer_disconnect_pty(void)
{
    char path[256];
    int master = serial_pty(path, sizeof(path));
    TEST_ASSERT(master >= 0);
    if (master < 0)
    {
        return;
    }
    SerialPort port;
    serial_port_init(&port);
    int ret = serial_port_open(&port, path, 115200, 1);
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        close(master);
        return;
    }
    close(master);
    unsigned char byte;
    int ready = serial_port_wait_readable(&port, 100);
    TEST_ASSERT(ready == -1 || ready == 1);
    if (ready == 1)
    {
        TEST_ASSERT(serial_port_read(&port, &byte, 1) <= 0);
    }
    TEST_ASSERT_EQ(serial_port_write_all(&port, "x", 1, 100), -1);
    serial_port_close(&port);
    TEST_ASSERT_EQ(port.fd, -1);
}
