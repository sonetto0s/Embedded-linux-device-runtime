#include "sysfs_io.h"

#include "test_framework.h"
#include <stdlib.h>
#include <unistd.h>

void test_sysfs_io_invalid(void)
{
    char buffer[32];
    long signed_value;
    unsigned long unsigned_value;
    unsigned long long ull_value;

    TEST_ASSERT_EQ(sysfs_read_text(NULL, buffer, sizeof(buffer)), -1);
    TEST_ASSERT_EQ(sysfs_read_text("/tmp/test", NULL, sizeof(buffer)), -1);
    TEST_ASSERT_EQ(sysfs_read_long(NULL, &signed_value), -1);
    TEST_ASSERT_EQ(sysfs_read_ulong(NULL, &unsigned_value), -1);
    TEST_ASSERT_EQ(sysfs_read_ull(NULL, &ull_value), -1);
    TEST_ASSERT_EQ(sysfs_write_text(NULL, "1"), -1);
}

void test_sysfs_io_text(void)
{
    char path[] = "/tmp/minishell_sysfs_text_XXXXXX";
    int fd = mkstemp(path);

    TEST_ASSERT(fd >= 0);

    if (fd < 0)
    {
        return;
    }

    close(fd);

    TEST_ASSERT_EQ(sysfs_write_text(path, "hello\n"), 0);

    char buffer[32];

    TEST_ASSERT_EQ(sysfs_read_text(path, buffer, sizeof(buffer)), 0);
    TEST_ASSERT_STR_EQ(buffer, "hello");

    unlink(path);
}

void test_sysfs_io_number(void)
{
    char path[] = "/tmp/minishell_sysfs_number_XXXXXX";
    int fd = mkstemp(path);

    TEST_ASSERT(fd >= 0);

    if (fd < 0)
    {
        return;
    }

    close(fd);

    TEST_ASSERT_EQ(sysfs_write_text(path, "1234567890123"), 0);

    unsigned long long value = 0;

    TEST_ASSERT_EQ(sysfs_read_ull(path, &value), 0);
    TEST_ASSERT(value == 1234567890123ULL);

    unlink(path);
}



