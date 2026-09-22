#include "runtime_snapshot.h"

#include "error.h"
#include "test_framework.h"

void test_runtime_snapshot_null(void)
{
    TEST_ASSERT_EQ(runtime_snapshot_collect(NULL), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_NULL(runtime_snapshot_hottest_thermal(NULL));
    TEST_ASSERT_NULL(runtime_snapshot_primary_network(NULL));
}

void test_runtime_snapshot_collect(void)
{
    RuntimeSnapshot snapshot;

    int ret = runtime_snapshot_collect(&snapshot);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT((snapshot.available_sources & RUNTIME_SNAPSHOT_SOURCE_SYSTEM) != 0);
    TEST_ASSERT((snapshot.failed_sources & RUNTIME_SNAPSHOT_SOURCE_SYSTEM) == 0);

    TEST_ASSERT(snapshot.runtime.cpu_usage >= 0.0);
    TEST_ASSERT(snapshot.runtime.cpu_usage <= 100.0);
    TEST_ASSERT(snapshot.runtime.memory_usage >= 0.0);
    TEST_ASSERT(snapshot.runtime.memory_usage <= 100.0);
    TEST_ASSERT(snapshot.runtime.process_count > 0);

    if (snapshot.available_sources & RUNTIME_SNAPSHOT_SOURCE_THERMAL)
    {
        TEST_ASSERT(snapshot.thermal.count <= THERMAL_MONITOR_MAX_ZONES);
    }

    if (snapshot.available_sources & RUNTIME_SNAPSHOT_SOURCE_NETWORK)
    {
        TEST_ASSERT(snapshot.network.count <= NETWORK_MONITOR_MAX_INTERFACES);
    }
}
