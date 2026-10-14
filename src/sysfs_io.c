#include "sysfs_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int sysfs_read_text(const char *path, char *buffer, size_t size)
{
    if (!path || !buffer || size < 2)
    {
        errno = EINVAL;
        return -1;
    }

    int fd = open(path, O_RDONLY | O_CLOEXEC);

    if (fd < 0)
    {
        return -1;
    }

    size_t total = 0;

    while (total < size - 1)
    {
        ssize_t count = read(fd, buffer + total, size - total - 1);

        if (count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return -1;
        }

        if (count == 0)
        {
            break;
        }

        total += (size_t)count;
    }

    if (close(fd) < 0)
    {
        return -1;
    }

    buffer[total] = '\0';

    while (total > 0 && (buffer[total - 1] == '\n' || buffer[total - 1] == '\r'))
    {
        buffer[total - 1] = '\0';
        total--;
    }

    return 0;
}

int sysfs_read_long(const char *path, long *value)
{
    if (!path || !value)
    {
        errno = EINVAL;
        return -1;
    }

    char buffer[64];

    if (sysfs_read_text(path, buffer, sizeof(buffer)) < 0)
    {
        return -1;
    }

    errno = 0;

    char *end = NULL;
    long result = strtol(buffer, &end, 10);

    if (errno != 0 || end == buffer || *end != '\0')
    {
        errno = EINVAL;
        return -1;
    }

    *value = result;

    return 0;
}

int sysfs_read_ulong(const char *path, unsigned long *value)
{
    if (!path || !value)
    {
        errno = EINVAL;
        return -1;
    }

    char buffer[64];

    if (sysfs_read_text(path, buffer, sizeof(buffer)) < 0)
    {
        return -1;
    }

    if (buffer[0] == '-')
    {
        errno = EINVAL;
        return -1;
    }

    errno = 0;

    char *end = NULL;
    unsigned long result = strtoul(buffer, &end, 10);

    if (errno != 0 || end == buffer || *end != '\0')
    {
        errno = EINVAL;
        return -1;
    }

    *value = result;

    return 0;
}

int sysfs_read_ull(const char *path, unsigned long long *value)
{
    if (!path || !value)
    {
        errno = EINVAL;
        return -1;
    }

    char buffer[64];

    if (sysfs_read_text(path, buffer, sizeof(buffer)) < 0)
    {
        return -1;
    }

    if (buffer[0] == '-')
    {
        errno = EINVAL;
        return -1;
    }

    errno = 0;

    char *end = NULL;
    unsigned long long result = strtoull(buffer, &end, 10);

    if (errno != 0 || end == buffer || *end != '\0')
    {
        errno = EINVAL;
        return -1;
    }

    *value = result;

    return 0;
}





int sysfs_write_text(const char *path, const char *value)
{
    if (!path || !value || !*value)
    {
        errno = EINVAL;
        return -1;
    }

    int fd = open(path, O_WRONLY | O_CLOEXEC);

    if (fd < 0)
    {
        return -1;
    }

    size_t length = strlen(value);
    ssize_t written;

    do
    {
        written = write(fd, value, length);
    }
    while (written < 0 && errno == EINTR);

    if (written < 0 || (size_t)written != length)
    {
        int saved_errno = written < 0 ? errno : EIO;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    if (close(fd) < 0)
    {
        return -1;
    }

    return 0;
}




