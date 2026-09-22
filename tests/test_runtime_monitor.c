#include "runtime_monitor.h"
#include "error.h"
#include "test_framework.h"
#include <string.h>

void test_runtime_monitor_collect_null(void)
{
    int ret = runtime_monitor_collect(NULL);

    TEST_ASSERT_EQ(ret, MiniShell_ERR_UNKNOWN);
}


void test_runtime_monitor_collect(void)
{
    RuntimeMonitor monitor;

    memset(&monitor, 0xAA, sizeof(monitor));

    int ret = runtime_monitor_collect(&monitor);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT(monitor.cpu_usage >= 0.0);
    TEST_ASSERT(monitor.cpu_usage <= 100.0);
    TEST_ASSERT(monitor.memory_usage >= 0.0);
    TEST_ASSERT(monitor.memory_usage <= 100.0);
    TEST_ASSERT(monitor.load_average[0] >= 0.0);
    TEST_ASSERT(monitor.load_average[1] >= 0.0);
    TEST_ASSERT(monitor.load_average[2] >= 0.0);
    TEST_ASSERT(monitor.process_count > 0);
    TEST_ASSERT(monitor.uptime >= 0.0);
}




