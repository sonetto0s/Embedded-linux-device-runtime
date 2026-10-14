#include "serial_port.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static int baud_to_speed(int baudrate, speed_t *speed)
{
    if (!speed)
    {
        errno = EINVAL;
        return -1;
    }

    switch (baudrate)
    {
        case 9600:
            *speed = B9600;
            return 0;
        case 19200:
            *speed = B19200;
            return 0;
        case 38400:
            *speed = B38400;
            return 0;
        case 57600:
            *speed = B57600;
            return 0;
        case 115200:
            *speed = B115200;
            return 0;
#ifdef B230400
        case 230400:
            *speed = B230400;
            return 0;
#endif
        default:
            errno = EINVAL;
            return -1;
    }
}

static int64_t monotonic_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
        return -1;
    }

    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int poll_fd(int fd, short events, int timeout_ms)
{
    if (timeout_ms < 0)
    {
        errno = EINVAL;
        return -1;
    }

    int64_t deadline = 0;

    if (timeout_ms > 0)
    {
        int64_t now = monotonic_ms();

        if (now < 0)
        {
            return -1;
        }

        deadline = now + timeout_ms;
    }

    for (;;)
    {
        int wait_ms = 0;

        if (timeout_ms > 0)
        {
            int64_t now = monotonic_ms();

            if (now < 0)
            {
                return -1;
            }

            int64_t remaining = deadline - now;

            if (remaining <= 0)
            {
                return 0;
            }

            if (remaining > INT_MAX)
            {
                remaining = INT_MAX;
            }

            wait_ms = (int)remaining;
        }

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = events;
        pfd.revents = 0;

        int result = poll(&pfd, 1, wait_ms);

        if (result > 0)
        {
            if (pfd.revents & events)
            {
                return 1;
            }

            if (pfd.revents & POLLNVAL)
            {
                errno = EBADF;
                return -1;
            }

            if (pfd.revents & (POLLERR | POLLHUP))
            {
                errno = EIO;
                return -1;
            }

            continue;
        }

        if (result == 0)
        {
            return 0;
        }

        if (errno != EINTR)
        {
            return -1;
        }
    }
}

static int configure_serial(int fd, int baudrate, int nonblocking)
{
    speed_t speed;

    if (baud_to_speed(baudrate, &speed) < 0)
    {
        return -1;
    }

    struct termios tty;

    if (tcgetattr(fd, &tty) < 0)
    {
        return -1;
    }

    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
    tty.c_cflag |= CS8 | CREAD | CLOCAL;

#ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;
#endif

    if (cfsetispeed(&tty, speed) < 0 || cfsetospeed(&tty, speed) < 0)
    {
        return -1;
    }

    tty.c_cc[VMIN] = nonblocking ? 0 : 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) < 0)
    {
        return -1;
    }

    return tcflush(fd, TCIOFLUSH);
}

void serial_port_init(SerialPort *port)
{
    if (!port)
    {
        return;
    }

    port->fd = -1;
}

int serial_port_open(SerialPort *port, const char *device_path, int baudrate, int nonblocking)
{
    if (!port || !device_path || !*device_path)
    {
        errno = EINVAL;
        return -1;
    }

    if (port->fd >= 0)
    {
        errno = EBUSY;
        return -1;
    }

    int flags = O_RDWR | O_NOCTTY | O_CLOEXEC;

    if (nonblocking)
    {
        flags |= O_NONBLOCK;
    }

    int fd = open(device_path, flags);

    if (fd < 0)
    {
        return -1;
    }

    if (configure_serial(fd, baudrate, nonblocking) < 0)
    {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    port->fd = fd;
    return 0;
}

ssize_t serial_port_read(SerialPort *port, void *buffer, size_t size)
{
    if (!port || port->fd < 0 || (!buffer && size > 0))
    {
        errno = EINVAL;
        return -1;
    }

    for (;;)
    {
        ssize_t result = read(port->fd, buffer, size);

        if (result < 0 && errno == EINTR)
        {
            continue;
        }

        return result;
    }
}

ssize_t serial_port_write(SerialPort *port, const void *buffer, size_t size)
{
    if (!port || port->fd < 0 || (!buffer && size > 0))
    {
        errno = EINVAL;
        return -1;
    }

    for (;;)
    {
        ssize_t result = write(port->fd, buffer, size);

        if (result < 0 && errno == EINTR)
        {
            continue;
        }

        return result;
    }
}

int serial_port_write_all(SerialPort *port, const void *buffer, size_t size, int timeout_ms)
{
    if (!port || port->fd < 0 || (!buffer && size > 0) || timeout_ms < 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (size == 0)
    {
        return 0;
    }

    int64_t now = monotonic_ms();

    if (now < 0)
    {
        return -1;
    }

    int64_t deadline = now + timeout_ms;
    const unsigned char *data = buffer;
    size_t total = 0;

    while (total < size)
    {
        now = monotonic_ms();

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

        if (remaining > INT_MAX)
        {
            remaining = INT_MAX;
        }

        int ready = poll_fd(port->fd, POLLOUT, (int)remaining);

        if (ready < 0)
        {
            return -1;
        }

        if (ready == 0)
        {
            errno = ETIMEDOUT;
            return -1;
        }

        ssize_t written = serial_port_write(port, data + total, size - total);

        if (written > 0)
        {
            total += (size_t)written;
            continue;
        }

        if (written == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        {
            continue;
        }

        return -1;
    }

    return 0;
}

int serial_port_wait_readable(SerialPort *port, int timeout_ms)
{
    if (!port || port->fd < 0 || timeout_ms < 0)
    {
        errno = EINVAL;
        return -1;
    }

    return poll_fd(port->fd, POLLIN, timeout_ms);
}

int serial_port_flush_input(SerialPort *port)
{
    if (!port || port->fd < 0)
    {
        errno = EINVAL;
        return -1;
    }

    while (tcflush(port->fd, TCIFLUSH) < 0)
    {
        if (errno != EINTR)
        {
            return -1;
        }
    }

    return 0;
}

void serial_port_close(SerialPort *port)
{
    if (!port)
    {
        return;
    }

    if (port->fd >= 0)
    {
        close(port->fd);
    }

    port->fd = -1;
}
