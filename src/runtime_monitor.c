#include "runtime_monitor.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>


static int read_memory(double *usage)
{
    FILE *fp = fopen("/proc/meminfo","r");

    if (!fp)
        return -1;


    char line[256];

    unsigned long total = 0;
    unsigned long available = 0;


    while (fgets(line,sizeof(line),fp))
    {
        if (sscanf(line,"MemTotal: %lu kB",&total) == 1)
            continue;


        if (sscanf(line,"MemAvailable: %lu kB",&available) == 1)
            continue;
    }


    fclose(fp);


    if (total == 0)
        return -1;


    *usage =
        (double)(total - available) * 100.0 /
        (double)total;


    return 0;
}



static int read_load(double load[3])
{
    FILE *fp = fopen("/proc/loadavg","r");

    if (!fp)
        return -1;


    int ret = fscanf(fp,
                     "%lf %lf %lf",
                     &load[0],
                     &load[1],
                     &load[2]);


    fclose(fp);


    return ret == 3 ? 0 : -1;
}



static int read_uptime(double *uptime)
{
    FILE *fp = fopen("/proc/uptime","r");

    if (!fp)
        return -1;


    int ret = fscanf(fp,"%lf",uptime);


    fclose(fp);


    return ret == 1 ? 0 : -1;
}



static unsigned int count_process(void)
{
    DIR *dir = opendir("/proc");

    if (!dir)
        return 0;


    unsigned int count = 0;


    struct dirent *entry;


    while ((entry = readdir(dir)))
    {
        int numeric = 1;


        for (size_t i = 0;
             entry->d_name[i];
             i++)
        {
            if (!isdigit(entry->d_name[i]))
            {
                numeric = 0;
                break;
            }
        }


        if (numeric)
            count++;
    }


    closedir(dir);


    return count;
}



static int read_cpu(double *usage)
{
    FILE *fp = fopen("/proc/stat","r");

    if (!fp)
        return -1;


    char line[256];


    if (!fgets(line,sizeof(line),fp))
    {
        fclose(fp);
        return -1;
    }


    fclose(fp);


    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;


    sscanf(line,
           "cpu %lu %lu %lu %lu",
           &user,
           &nice,
           &system,
           &idle);



    unsigned long total =
        user + nice + system + idle;


    if (!total)
        return -1;


    *usage =
        (double)(total-idle)*100.0/
        (double)total;


    return 0;
}



int runtime_monitor_collect(RuntimeMonitor *monitor)
{
    if (!monitor)
        return -1;


    memset(monitor,0,sizeof(*monitor));


    if (read_cpu(&monitor->cpu_usage))
        return -1;


    if (read_memory(&monitor->memory_usage))
        return -1;


    if (read_load(monitor->load_average))
        return -1;


    if (read_uptime(&monitor->uptime))
        return -1;


    monitor->process_count =
        count_process();


    return 0;
}



void runtime_monitor_print(const RuntimeMonitor *monitor)
{
    if (!monitor)
        return;


    printf("\n");

    printf("========== Runtime Monitor ==========\n");


    printf("CPU Usage       : %.1f %%\n",
           monitor->cpu_usage);


    printf("Memory Usage    : %.1f %%\n",
           monitor->memory_usage);


    printf("Load Average    : %.2f %.2f %.2f\n",
           monitor->load_average[0],
           monitor->load_average[1],
           monitor->load_average[2]);


    printf("Process Count   : %u\n",
           monitor->process_count);


    printf("Uptime          : %.2f seconds\n",
           monitor->uptime);


    printf("=====================================\n\n");
}
