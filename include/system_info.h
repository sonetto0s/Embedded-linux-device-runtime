#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

typedef struct
{
    char board[128];
    char kernel[64];
    char hostname[64];
    char architecture[32];
    char cpu_model[128];
    long cpu_cores;
    unsigned long mem_total;
    unsigned long mem_available;
    double uptime;
    double soc_temperature;
    int has_soc_temperature;
} SystemInfo;

int system_info_collect(SystemInfo *info);
void system_info_print(SystemInfo *info);

#endif

