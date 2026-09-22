#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

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

int process_manager_get(pid_t pid, ProcessInfo *info);
int process_manager_collect(ProcessInfo **list, size_t *count);
void process_manager_print(const ProcessInfo *list, size_t count);
void process_manager_print_one(const ProcessInfo *info);
void process_manager_free(ProcessInfo *list);

#endif



