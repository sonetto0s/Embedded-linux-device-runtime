#ifndef RUNTIME_MONITOR_H
#define RUNTIME_MONITOR_H

typedef struct
{
    double cpu_usage;
    double memory_usage;
    double load_average[3];
    unsigned int process_count;
    double uptime;
} RuntimeMonitor;

int runtime_monitor_collect(RuntimeMonitor *monitor);
void runtime_monitor_print(const RuntimeMonitor *monitor);

#endif

