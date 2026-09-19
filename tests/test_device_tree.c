#include "device_tree.h"
#include "error.h"
#include "test_framework.h"

void test_device_tree_collect_null(void)
{
    int ret = device_tree_collect(NULL);
    TEST_ASSERT_EQ(ret, MiniShell_ERR_UNKNOWN);
}

void test_device_tree_collect(void)
{
    DeviceTreeInfo info;
    int ret = device_tree_collect(&info);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT(info.compatible_count <= DEVICE_TREE_MAX_COMPATIBLE);

    if (info.available)
    {
        TEST_ASSERT(info.model[0] != '\0' || info.compatible_count > 0);
    }
}
