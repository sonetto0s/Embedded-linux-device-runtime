#ifndef RUNTIME_SNAPSHOT_H
#define RUNTIME_SNAPSHOT_H

#include "network_monitor.h"
#include "runtime_monitor.h"
#include "thermal_monitor.h"

enum
{
    RUNTIME_SNAPSHOT_SOURCE_SYSTEM = 1u << 0,
    RUNTIME_SNAPSHOT_SOURCE_THERMAL = 1u << 1,
    RUNTIME_SNAPSHOT_SOURCE_NETWORK = 1u << 2
};

typedef struct
{
    RuntimeMonitor runtime;
    ThermalMonitor thermal;
    NetworkMonitor network;

    unsigned int available_sources;
    unsigned int failed_sources;
} RuntimeSnapshot;

int runtime_snapshot_collect(RuntimeSnapshot *snapshot);
const ThermalRuntimeInfo *runtime_snapshot_hottest_thermal(const RuntimeSnapshot *snapshot);
const NetworkRuntimeInfo *runtime_snapshot_primary_network(const RuntimeSnapshot *snapshot);

#endif
