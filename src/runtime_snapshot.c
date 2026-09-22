#include "runtime_snapshot.h"

#include "error.h"
#include <string.h>

int runtime_snapshot_collect(RuntimeSnapshot *snapshot)
{
    if (!snapshot)
    {
        return MiniShell_ERR_UNKNOWN;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    int ret = runtime_monitor_collect(&snapshot->runtime);

    if (ret != MiniShell_OK)
    {
        snapshot->failed_sources |= RUNTIME_SNAPSHOT_SOURCE_SYSTEM;
        return ret;
    }

    snapshot->available_sources |= RUNTIME_SNAPSHOT_SOURCE_SYSTEM;

    ret = thermal_monitor_collect(&snapshot->thermal);

    if (ret == MiniShell_OK)
    {
        snapshot->available_sources |= RUNTIME_SNAPSHOT_SOURCE_THERMAL;
    }
    else
    {
        snapshot->failed_sources |= RUNTIME_SNAPSHOT_SOURCE_THERMAL;
    }

    ret = network_monitor_collect(&snapshot->network);

    if (ret == MiniShell_OK)
    {
        snapshot->available_sources |= RUNTIME_SNAPSHOT_SOURCE_NETWORK;
    }
    else
    {
        snapshot->failed_sources |= RUNTIME_SNAPSHOT_SOURCE_NETWORK;
    }

    return MiniShell_OK;
}

const ThermalRuntimeInfo *runtime_snapshot_hottest_thermal(const RuntimeSnapshot *snapshot)
{
    if (!snapshot || !(snapshot->available_sources & RUNTIME_SNAPSHOT_SOURCE_THERMAL))
    {
        return NULL;
    }

    return thermal_monitor_hottest(&snapshot->thermal);
}

const NetworkRuntimeInfo *runtime_snapshot_primary_network(const RuntimeSnapshot *snapshot)
{
    if (!snapshot || !(snapshot->available_sources & RUNTIME_SNAPSHOT_SOURCE_NETWORK))
    {
        return NULL;
    }

    return network_monitor_primary(&snapshot->network);
}
