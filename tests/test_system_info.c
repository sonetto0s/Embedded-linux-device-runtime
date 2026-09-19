#include "system_info.h"
#include "error.h"
#include "test_framework.h"
#include <string.h>

void test_system_info_collect_null(void)
{
    int ret = system_info_collect(NULL);
    TEST_ASSERT_EQ(ret, MiniShell_ERR_UNKNOWN);
}

void test_system_info_collect(void)
{
    SystemInfo info;
    int ret = system_info_collect(&info);
    TEST_ASSERT_EQ(ret, MiniShell_OK);
}

void test_system_info_fields(void)
{
    SystemInfo info;
    int ret = system_info_collect(&info);
    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT(info.kernel[0] != '\0');
    TEST_ASSERT(info.hostname[0] != '\0');
    TEST_ASSERT(info.architecture[0] != '\0');
    TEST_ASSERT(info.cpu_cores > 0);
    TEST_ASSERT(info.mem_total > 0);
    TEST_ASSERT(info.mem_available > 0);
    TEST_ASSERT(info.mem_available <= info.mem_total);
    TEST_ASSERT(info.uptime >= 0.0);
}

void test_system_info_collect_overwrite(void)
{
    SystemInfo info;
    memset(&info, 0xAA, sizeof(info));

    int ret = system_info_collect(&info);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT(info.kernel[0] != '\0');
    TEST_ASSERT(info.hostname[0] != '\0');
    TEST_ASSERT(info.architecture[0] != '\0');
    TEST_ASSERT(info.cpu_cores > 0);
    TEST_ASSERT(info.mem_total > 0);
    TEST_ASSERT(info.uptime >= 0.0);
}
























