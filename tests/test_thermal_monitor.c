#include "thermal_monitor.h"

#include "error.h"
#include "test_framework.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_fixture(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");

    if (!fp)
    {
        return -1;
    }

    int ret = fputs(text, fp) < 0 ? -1 : 0;

    if (fclose(fp) != 0)
    {
        return -1;
    }

    return ret;
}

void test_thermal_monitor_invalid(void)
{
    ThermalMonitor monitor;

    TEST_ASSERT_EQ(thermal_monitor_collect_from(NULL, "/tmp"), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(thermal_monitor_collect_from(&monitor, NULL), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(thermal_monitor_collect_from(&monitor, ""), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(thermal_monitor_collect_from(&monitor, "/tmp/minishell_missing_thermal"), MiniShell_ERR_OPEN);
    TEST_ASSERT_NULL(thermal_monitor_hottest(NULL));
}

void test_thermal_monitor_fixture(void)
{
    char base[] = "/tmp/minishell_thermal_XXXXXX";

    TEST_ASSERT_NOT_NULL(mkdtemp(base));

    char zone0[PATH_MAX];
    char zone1[PATH_MAX];
    char path[PATH_MAX];

    snprintf(zone0, sizeof(zone0), "%s/thermal_zone0", base);
    snprintf(zone1, sizeof(zone1), "%s/thermal_zone1", base);

    TEST_ASSERT_EQ(mkdir(zone0, 0700), 0);
    TEST_ASSERT_EQ(mkdir(zone1, 0700), 0);

    snprintf(path, sizeof(path), "%s/thermal_zone0/type", base);
    TEST_ASSERT_EQ(write_fixture(path, "soc-thermal\n"), 0);

    snprintf(path, sizeof(path), "%s/thermal_zone0/temp", base);
    TEST_ASSERT_EQ(write_fixture(path, "42000\n"), 0);

    snprintf(path, sizeof(path), "%s/thermal_zone1/type", base);
    TEST_ASSERT_EQ(write_fixture(path, "cpu-thermal\n"), 0);

    snprintf(path, sizeof(path), "%s/thermal_zone1/temp", base);
    TEST_ASSERT_EQ(write_fixture(path, "55000\n"), 0);

    ThermalMonitor monitor;
    int ret = thermal_monitor_collect_from(&monitor, base);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT_EQ(monitor.count, 2);
    TEST_ASSERT_STR_EQ(monitor.zones[0].name, "thermal_zone0");
    TEST_ASSERT_STR_EQ(monitor.zones[0].type, "soc-thermal");
    TEST_ASSERT(monitor.zones[0].has_temperature);
    TEST_ASSERT(monitor.zones[0].temperature == 42.0);

    const ThermalRuntimeInfo *hottest = thermal_monitor_hottest(&monitor);

    TEST_ASSERT_NOT_NULL(hottest);

    if (hottest)
    {
        TEST_ASSERT_STR_EQ(hottest->type, "cpu-thermal");
        TEST_ASSERT(hottest->temperature == 55.0);
    }

    snprintf(path, sizeof(path), "%s/thermal_zone0/type", base);
    unlink(path);
    snprintf(path, sizeof(path), "%s/thermal_zone0/temp", base);
    unlink(path);
    snprintf(path, sizeof(path), "%s/thermal_zone1/type", base);
    unlink(path);
    snprintf(path, sizeof(path), "%s/thermal_zone1/temp", base);
    unlink(path);

    rmdir(zone0);
    rmdir(zone1);
    rmdir(base);
}
