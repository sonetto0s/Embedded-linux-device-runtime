#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <stddef.h>
#include <sys/types.h>

#define PROCESS_NAME_LENGTH 64

typedef struct
{
    pid_t pid;
    char name[PROCESS_NAME_LENGTH];
    char state;
    unsigned long rss_kb;
    unsigned int threads;
} ProcessInfo;

int process_monitor_get(pid_t pid, ProcessInfo *info);
int process_monitor_collect(ProcessInfo **list, size_t *count);
void process_monitor_free(ProcessInfo *list);

#endif



