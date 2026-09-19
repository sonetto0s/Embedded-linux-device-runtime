#define _GNU_SOURCE
#include "system_info.h"
#include "error.h"
#include "log.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

static void get_board_info(SystemInfo *info);
static int get_cpu_info(SystemInfo *info);
static int get_mem_info(SystemInfo *info);
static int get_uptime_info(SystemInfo *info);
static void get_thermal_info(SystemInfo *info);

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0 || !src)
        return;

    size_t length = strcspn(src, "\r\n");

    if (length >= dst_size)
        length = dst_size - 1;

    memcpy(dst, src, length);
    dst[length] = '\0';
}

static int cpuinfo_value(const char *line,
                         const char *key,
                         char *value,
                         size_t value_size)
{
    const char *colon = strchr(line, ':');

    if (!colon)
        return 0;

    const char *key_end = colon;

    while (key_end > line &&
           (key_end[-1] == ' ' || key_end[-1] == '\t')) {
        key_end--;
    }

    size_t key_length = (size_t)(key_end - line);

    if (strlen(key) != key_length ||
        strncmp(line, key, key_length) != 0) {
        return 0;
    }

    const char *text = colon + 1;

    while (*text == ' ' || *text == '\t')
        text++;

    copy_text(value, value_size, text);

    return value[0] != '\0';
}

static int read_device_tree_model(char *buffer, size_t size)
{
    if (!buffer || size == 0)
        return -1;

    FILE *fp = fopen("/proc/device-tree/model", "rb");

    if (!fp)
        return -1;

    size_t n = fread(buffer, 1, size - 1, fp);

    fclose(fp);

    if (n == 0)
        return -1;

    buffer[n] = '\0';
    buffer[strcspn(buffer, "\r\n")] = '\0';

    return buffer[0] == '\0' ? -1 : 0;
}

int system_info_collect(SystemInfo *info)
{
    if (!info) {
        log_error("system_info is null");
        return MiniShell_ERR_UNKNOWN;
    }

    memset(info, 0, sizeof(SystemInfo));

    struct utsname uts;

    if (uname(&uts) < 0) {
        log_error("failed uname");
        return MiniShell_ERR_UNKNOWN;
    }

    copy_text(info->kernel,
              sizeof(info->kernel),
              uts.release);

    copy_text(info->architecture,
              sizeof(info->architecture),
              uts.machine);

    if (gethostname(info->hostname,
                    sizeof(info->hostname) - 1) < 0) {
        log_error("gethostname failed");
        return MiniShell_ERR_UNKNOWN;
    }

    info->hostname[sizeof(info->hostname) - 1] = '\0';

    get_board_info(info);

    int ret = get_cpu_info(info);

    if (ret != MiniShell_OK)
        return ret;

    ret = get_mem_info(info);

    if (ret != MiniShell_OK)
        return ret;

    ret = get_uptime_info(info);

    if (ret != MiniShell_OK)
        return ret;

    get_thermal_info(info);

    return MiniShell_OK;
}

void system_info_print(SystemInfo *info)
{
    if (!info) {
        log_error("system_info is null");
        return;
    }

    printf("\n");
    printf("========== System ==========\n");

    printf("Board            : %s\n",
           info->board[0] ? info->board : "N/A");
    printf("Kernel           : %s\n", info->kernel);
    printf("Hostname         : %s\n", info->hostname);
    printf("Architecture     : %s\n", info->architecture);
    printf("CPU Model        : %s\n",
           info->cpu_model[0] ? info->cpu_model : "N/A");

    if (info->cpu_cores > 0)
        printf("CPU Cores        : %ld\n", info->cpu_cores);
    else
        printf("CPU Cores        : N/A\n");

    if (info->has_soc_temperature)
        printf("SoC Temperature  : %.1f C\n", info->soc_temperature);
    else
        printf("SoC Temperature  : N/A\n");

    printf("Memory Total     : %lu MB\n", info->mem_total);
    printf("Memory Available : %lu MB\n", info->mem_available);
    printf("Uptime           : %.2f seconds\n", info->uptime);

    printf("\n");
}

static void get_board_info(SystemInfo *info)
{
    read_device_tree_model(info->board,
                           sizeof(info->board));
}

static int get_cpu_info(SystemInfo *info)
{
    FILE *fp = fopen("/proc/cpuinfo", "r");

    if (!fp) {
        log_error("open /proc/cpuinfo failed");
        return MiniShell_ERR_OPEN;
    }

    char line[256];
    char candidate[sizeof(info->cpu_model)] = {0};
    int best_rank = 100;

    while (fgets(line, sizeof(line), fp)) {
        struct {
            const char *key;
            int rank;
        } keys[] = {
            {"model name", 0},
            {"Model", 1},
            {"Hardware", 2},
            {"Processor", 3}
        };

        for (size_t i = 0;
             i < sizeof(keys) / sizeof(keys[0]);
             i++) {

            if (keys[i].rank >= best_rank)
                continue;

            if (cpuinfo_value(line,
                              keys[i].key,
                              candidate,
                              sizeof(candidate))) {

                copy_text(info->cpu_model,
                          sizeof(info->cpu_model),
                          candidate);

                best_rank = keys[i].rank;
            }
        }
    }

    fclose(fp);

    info->cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (info->cpu_cores < 0)
        info->cpu_cores = 0;

    return MiniShell_OK;
}

static int get_mem_info(SystemInfo *info)
{
    FILE *fp = fopen("/proc/meminfo", "r");

    if (!fp) {
        log_error("open /proc/meminfo failed");
        return MiniShell_ERR_OPEN;
    }

    char line[256];
    int got_total = 0;
    int got_available = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            if (sscanf(line,
                       "MemTotal:%lu",
                       &info->mem_total) == 1) {
                got_total = 1;
            }
        } else if (strncmp(line,
                           "MemAvailable:",
                           13) == 0) {

            if (sscanf(line,
                       "MemAvailable:%lu",
                       &info->mem_available) == 1) {
                got_available = 1;
            }
        }
    }

    fclose(fp);

    if (!got_total || !got_available) {
        log_error("failed read memory information");
        return MiniShell_ERR_UNKNOWN;
    }

    info->mem_total /= 1024;
    info->mem_available /= 1024;

    return MiniShell_OK;
}

static int get_uptime_info(SystemInfo *info)
{
    FILE *fp = fopen("/proc/uptime", "r");

    if (!fp) {
        log_error("open /proc/uptime failed");
        return MiniShell_ERR_OPEN;
    }

    if (fscanf(fp, "%lf", &info->uptime) != 1) {
        fclose(fp);
        log_error("failed read uptime");
        return MiniShell_ERR_UNKNOWN;
    }

    fclose(fp);

    return MiniShell_OK;
}

static void get_thermal_info(SystemInfo *info)
{
    DIR *dir = opendir("/sys/class/thermal");

    if (!dir)
        return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name,
                    "thermal_zone",
                    sizeof("thermal_zone") - 1) != 0) {
            continue;
        }

        char type_path[256];

        int written = snprintf(type_path,
                               sizeof(type_path),
                               "/sys/class/thermal/%s/type",
                               entry->d_name);

        if (written < 0 ||
            (size_t)written >= sizeof(type_path)) {
            continue;
        }

        FILE *type_fp = fopen(type_path, "r");

        if (!type_fp)
            continue;

        char type[64];

        if (!fgets(type, sizeof(type), type_fp)) {
            fclose(type_fp);
            continue;
        }

        fclose(type_fp);

        type[strcspn(type, "\r\n")] = '\0';

        if (strcmp(type, "soc-thermal") != 0)
            continue;

        char temp_path[256];

        written = snprintf(temp_path, sizeof(temp_path), "/sys/class/thermal/%s/temp", entry->d_name);

        if (written < 0 || (size_t)written >= sizeof(temp_path))
        {
            continue;
        }

        FILE *temp_fp = fopen(temp_path, "r");

        if (!temp_fp)
            continue;

        long temperature;

        if (fscanf(temp_fp, "%ld", &temperature) == 1)
        {
            info->soc_temperature = (double)temperature / 1000.0;
            info->has_soc_temperature = 1;
        }

        fclose(temp_fp);

        if (info->has_soc_temperature)
            break;
    }

    closedir(dir);
}





