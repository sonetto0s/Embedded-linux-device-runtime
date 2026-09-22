#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <stddef.h>

#define NETWORK_MONITOR_MAX_INTERFACES 16

typedef struct
{
    char name[64];
    char state[32];

    unsigned long long rx_bytes;
    unsigned long long tx_bytes;

    int has_rx;
    int has_tx;
} NetworkRuntimeInfo;

typedef struct
{
    NetworkRuntimeInfo interfaces[NETWORK_MONITOR_MAX_INTERFACES];
    size_t count;
} NetworkMonitor;

int network_monitor_collect(NetworkMonitor *monitor);
int network_monitor_collect_from(NetworkMonitor *monitor, const char *base);
const NetworkRuntimeInfo *network_monitor_primary(const NetworkMonitor *monitor);

#endif
