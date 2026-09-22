#ifndef THERMAL_MONITOR_H
#define THERMAL_MONITOR_H

#include <stddef.h>

#define THERMAL_MONITOR_MAX_ZONES 16

typedef struct
{
    char name[64];
    char type[64];

    double temperature;
    int has_temperature;
} ThermalRuntimeInfo;

typedef struct
{
    ThermalRuntimeInfo zones[THERMAL_MONITOR_MAX_ZONES];
    size_t count;
} ThermalMonitor;

int thermal_monitor_collect(ThermalMonitor *monitor);
int thermal_monitor_collect_from(ThermalMonitor *monitor, const char *base);
const ThermalRuntimeInfo *thermal_monitor_hottest(const ThermalMonitor *monitor);

#endif
