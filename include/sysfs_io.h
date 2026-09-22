#ifndef SYSFS_IO_H
#define SYSFS_IO_H

#include <stddef.h>

int sysfs_read_text(const char *path, char *buffer, size_t size);
int sysfs_read_long(const char *path, long *value);
int sysfs_read_ulong(const char *path, unsigned long *value);
int sysfs_read_ull(const char *path, unsigned long long *value);
int sysfs_write_text(const char *path, const char *value);

#endif


