#include "hardware_info.h"
#include "error.h"
#include "test_framework.h"

void test_hardware_info_collect_null(void)
{
    int ret = hardware_info_collect(NULL);
    TEST_ASSERT_EQ(ret, MiniShell_ERR_UNKNOWN);
}

void test_hardware_info_collect(void)
{
    HardwareInfo info;
    int ret = hardware_info_collect(&info);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT(info.thermal_count <= HARDWARE_MAX_THERMAL_ZONES);
    TEST_ASSERT(info.cpu_policy_count <= HARDWARE_MAX_CPU_POLICIES);
    TEST_ASSERT(info.network_count <= HARDWARE_MAX_NETWORK_INTERFACES);
    TEST_ASSERT(info.led_count <= HARDWARE_MAX_LEDS);

    if (info.thermal_count > 0)
    {
        TEST_ASSERT(info.thermal_zones[0].name[0] != '\0');
    }

    if (info.cpu_policy_count > 0)
    {
        TEST_ASSERT(info.cpu_policies[0].name[0] != '\0');
    }

    if (info.network_count > 0)
    {
        TEST_ASSERT(info.network_interfaces[0].name[0] != '\0');
    }

    if (info.led_count > 0)
    {
        TEST_ASSERT(info.leds[0].name[0] != '\0');
    }
}
