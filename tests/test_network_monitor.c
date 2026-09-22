#include "network_monitor.h"

#include "error.h"
#include "test_framework.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void test_network_monitor_invalid(void)
{
    NetworkMonitor monitor;

    TEST_ASSERT_EQ(network_monitor_collect_from(NULL, "/tmp"), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(network_monitor_collect_from(&monitor, NULL), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(network_monitor_collect_from(&monitor, ""), MiniShell_ERR_UNKNOWN);
    TEST_ASSERT_EQ(network_monitor_collect_from(&monitor, "/tmp/minishell_missing_network"), MiniShell_ERR_OPEN);
    TEST_ASSERT_NULL(network_monitor_primary(NULL));
}

void test_network_monitor_fixture(void)
{
    char base[] = "/tmp/minishell_network_XXXXXX";

    TEST_ASSERT_NOT_NULL(mkdtemp(base));

    char eth0[PATH_MAX];
    char eth0_stats[PATH_MAX];
    char lo[PATH_MAX];
    char lo_stats[PATH_MAX];
    char path[PATH_MAX];

    snprintf(eth0, sizeof(eth0), "%s/eth0", base);
    snprintf(eth0_stats, sizeof(eth0_stats), "%s/eth0/statistics", base);
    snprintf(lo, sizeof(lo), "%s/lo", base);
    snprintf(lo_stats, sizeof(lo_stats), "%s/lo/statistics", base);

    TEST_ASSERT_EQ(mkdir(eth0, 0700), 0);
    TEST_ASSERT_EQ(mkdir(eth0_stats, 0700), 0);
    TEST_ASSERT_EQ(mkdir(lo, 0700), 0);
    TEST_ASSERT_EQ(mkdir(lo_stats, 0700), 0);

    snprintf(path, sizeof(path), "%s/eth0/operstate", base);
    TEST_ASSERT_EQ(write_fixture(path, "up\n"), 0);

    snprintf(path, sizeof(path), "%s/eth0/statistics/rx_bytes", base);
    TEST_ASSERT_EQ(write_fixture(path, "123456\n"), 0);

    snprintf(path, sizeof(path), "%s/eth0/statistics/tx_bytes", base);
    TEST_ASSERT_EQ(write_fixture(path, "654321\n"), 0);

    snprintf(path, sizeof(path), "%s/lo/operstate", base);
    TEST_ASSERT_EQ(write_fixture(path, "unknown\n"), 0);

    snprintf(path, sizeof(path), "%s/lo/statistics/rx_bytes", base);
    TEST_ASSERT_EQ(write_fixture(path, "100\n"), 0);

    snprintf(path, sizeof(path), "%s/lo/statistics/tx_bytes", base);
    TEST_ASSERT_EQ(write_fixture(path, "100\n"), 0);

    NetworkMonitor monitor;
    int ret = network_monitor_collect_from(&monitor, base);

    TEST_ASSERT_EQ(ret, MiniShell_OK);
    TEST_ASSERT_EQ(monitor.count, 2);
    TEST_ASSERT_STR_EQ(monitor.interfaces[0].name, "eth0");
    TEST_ASSERT_STR_EQ(monitor.interfaces[0].state, "up");
    TEST_ASSERT(monitor.interfaces[0].has_rx);
    TEST_ASSERT(monitor.interfaces[0].has_tx);
    TEST_ASSERT(monitor.interfaces[0].rx_bytes == 123456ULL);
    TEST_ASSERT(monitor.interfaces[0].tx_bytes == 654321ULL);

    const NetworkRuntimeInfo *primary = network_monitor_primary(&monitor);

    TEST_ASSERT_NOT_NULL(primary);

    if (primary)
    {
        TEST_ASSERT_STR_EQ(primary->name, "eth0");
    }

    snprintf(path, sizeof(path), "%s/eth0/operstate", base);
    unlink(path);
    snprintf(path, sizeof(path), "%s/eth0/statistics/rx_bytes", base);
    unlink(path);
    snprintf(path, sizeof(path), "%s/eth0/statistics/tx_bytes", base);
    unlink(path);
    snprintf(path, sizeof(path), "%s/lo/operstate", base);
    unlink(path);
    snprintf(path, sizeof(path), "%s/lo/statistics/rx_bytes", base);
    unlink(path);
    snprintf(path, sizeof(path), "%s/lo/statistics/tx_bytes", base);
    unlink(path);

    rmdir(eth0_stats);
    rmdir(eth0);
    rmdir(lo_stats);
    rmdir(lo);
    rmdir(base);
}
