#include "hardware_info.h"
#include "error.h"
#include "log.h"
#include "sysfs_io.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0 || !src)
    {
        return;
    }

    size_t length = strlen(src);

    if (length >= dst_size)
    {
        length = dst_size - 1;
    }

    memcpy(dst, src, length);
    dst[length] = '\0';
}

static int build_path(char *buffer, size_t size, const char *base, const char *entry, const char *file)
{
    int written = snprintf(buffer, size, "%s/%s/%s", base, entry, file);

    if (written < 0 || (size_t)written >= size)
    {
        return -1;
    }

    return 0;
}

static int read_led_trigger(const char *path, char *buffer, size_t size)
{
    char line[2048];

    if (sysfs_read_text(path, line, sizeof(line)) < 0)
    {
        return -1;
    }

    char *begin = strchr(line, '[');

    if (!begin)
    {
        return -1;
    }

    char *end = strchr(begin, ']');

    if (!end || end <= begin + 1)
    {
        return -1;
    }

    size_t length = (size_t)(end - begin - 1);

    if (length >= size)
    {
        length = size - 1;
    }

    memcpy(buffer, begin + 1, length);
    buffer[length] = '\0';

    return 0;
}

static void collect_thermal(HardwareInfo *info)
{
    const char *base = "/sys/class/thermal";
    DIR *dir = opendir(base);

    if (!dir)
    {
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "thermal_zone", sizeof("thermal_zone") - 1) != 0)
        {
            continue;
        }

        if (info->thermal_count >= HARDWARE_MAX_THERMAL_ZONES)
        {
            break;
        }

        ThermalZoneInfo *zone = &info->thermal_zones[info->thermal_count];
        char path[512];

        copy_text(zone->name, sizeof(zone->name), entry->d_name);

        if (build_path(path, sizeof(path), base, entry->d_name, "type") < 0)
        {
            continue;
        }

        if (sysfs_read_text(path, zone->type, sizeof(zone->type)) < 0)
        {
            continue;
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "temp") == 0)
        {
            long temperature;

            if (sysfs_read_long(path, &temperature) == 0)
            {
                zone->temperature = (double)temperature / 1000.0;
                zone->has_temperature = 1;
            }
        }

        info->thermal_count++;
    }

    closedir(dir);
}

static void collect_cpu_frequency(HardwareInfo *info)
{
    const char *base = "/sys/devices/system/cpu/cpufreq";
    DIR *dir = opendir(base);

    if (!dir)
    {
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "policy", sizeof("policy") - 1) != 0)
        {
            continue;
        }

        if (info->cpu_policy_count >= HARDWARE_MAX_CPU_POLICIES)
        {
            break;
        }

        CpuFreqPolicyInfo *policy = &info->cpu_policies[info->cpu_policy_count];
        char path[512];

        copy_text(policy->name, sizeof(policy->name), entry->d_name);

        if (build_path(path, sizeof(path), base, entry->d_name, "affected_cpus") == 0)
        {
            sysfs_read_text(path, policy->affected_cpus, sizeof(policy->affected_cpus));
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "scaling_driver") == 0)
        {
            sysfs_read_text(path, policy->driver, sizeof(policy->driver));
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "scaling_governor") == 0)
        {
            sysfs_read_text(path, policy->governor, sizeof(policy->governor));
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "scaling_cur_freq") == 0)
        {
            if (sysfs_read_ulong(path, &policy->current_khz) == 0)
            {
                policy->has_current = 1;
            }
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "scaling_min_freq") == 0)
        {
            if (sysfs_read_ulong(path, &policy->scaling_min_khz) == 0)
            {
                policy->has_scaling_min = 1;
            }
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "scaling_max_freq") == 0)
        {
            if (sysfs_read_ulong(path, &policy->scaling_max_khz) == 0)
            {
                policy->has_scaling_max = 1;
            }
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "cpuinfo_min_freq") == 0)
        {
            if (sysfs_read_ulong(path, &policy->hardware_min_khz) == 0)
            {
                policy->has_hardware_min = 1;
            }
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "cpuinfo_max_freq") == 0)
        {
            if (sysfs_read_ulong(path, &policy->hardware_max_khz) == 0)
            {
                policy->has_hardware_max = 1;
            }
        }

        info->cpu_policy_count++;
    }

    closedir(dir);
}

static void collect_network(HardwareInfo *info)
{
    const char *base = "/sys/class/net";
    DIR *dir = opendir(base);

    if (!dir)
    {
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
        {
            continue;
        }

        if (info->network_count >= HARDWARE_MAX_NETWORK_INTERFACES)
        {
            break;
        }

        NetworkInterfaceInfo *network = &info->network_interfaces[info->network_count];
        char path[512];

        copy_text(network->name, sizeof(network->name), entry->d_name);

        if (build_path(path, sizeof(path), base, entry->d_name, "operstate") == 0)
        {
            sysfs_read_text(path, network->state, sizeof(network->state));
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "address") == 0)
        {
            sysfs_read_text(path, network->address, sizeof(network->address));
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "mtu") == 0)
        {
            if (sysfs_read_ulong(path, &network->mtu) == 0)
            {
                network->has_mtu = 1;
            }
        }

        info->network_count++;
    }

    closedir(dir);
}

static void collect_leds(HardwareInfo *info)
{
    const char *base = "/sys/class/leds";
    DIR *dir = opendir(base);

    if (!dir)
    {
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
        {
            continue;
        }

        if (info->led_count >= HARDWARE_MAX_LEDS)
        {
            break;
        }

        LedInfo *led = &info->leds[info->led_count];
        char path[512];

        copy_text(led->name, sizeof(led->name), entry->d_name);

        if (build_path(path, sizeof(path), base, entry->d_name, "brightness") == 0)
        {
            if (sysfs_read_long(path, &led->brightness) == 0)
            {
                led->has_brightness = 1;
            }
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "max_brightness") == 0)
        {
            if (sysfs_read_long(path, &led->max_brightness) == 0)
            {
                led->has_max_brightness = 1;
            }
        }

        if (build_path(path, sizeof(path), base, entry->d_name, "trigger") == 0)
        {
            read_led_trigger(path, led->trigger, sizeof(led->trigger));
        }

        info->led_count++;
    }

    closedir(dir);
}

int hardware_info_collect(HardwareInfo *info)
{
    if (!info)
    {
        log_error("hardware_info is null");
        return MiniShell_ERR_UNKNOWN;
    }

    memset(info, 0, sizeof(*info));

    collect_thermal(info);
    collect_cpu_frequency(info);
    collect_network(info);
    collect_leds(info);

    return MiniShell_OK;
}

static void print_thermal(const HardwareInfo *info)
{
    printf("Thermal Zones:\n");

    if (info->thermal_count == 0)
    {
        printf("  N/A\n");
        return;
    }

    for (size_t i = 0; i < info->thermal_count; i++)
    {
        const ThermalZoneInfo *zone = &info->thermal_zones[i];

        if (zone->has_temperature)
        {
            printf("  %-14s %-20s %.1f C\n", zone->name, zone->type[0] ? zone->type : "N/A", zone->temperature);
        }
        else
        {
            printf("  %-14s %-20s N/A\n", zone->name, zone->type[0] ? zone->type : "N/A");
        }
    }
}

static void print_cpu_frequency(const HardwareInfo *info)
{
    printf("\nCPU Frequency:\n");

    if (info->cpu_policy_count == 0)
    {
        printf("  N/A\n");
        return;
    }

    for (size_t i = 0; i < info->cpu_policy_count; i++)
    {
        const CpuFreqPolicyInfo *policy = &info->cpu_policies[i];

        printf("  %s\n", policy->name);
        printf("    CPUs     : %s\n", policy->affected_cpus[0] ? policy->affected_cpus : "N/A");
        printf("    Driver   : %s\n", policy->driver[0] ? policy->driver : "N/A");
        printf("    Governor : %s\n", policy->governor[0] ? policy->governor : "N/A");

        if (policy->has_current)
        {
            printf("    Current  : %.1f MHz\n", (double)policy->current_khz / 1000.0);
        }
        else
        {
            printf("    Current  : N/A\n");
        }

        if (policy->has_scaling_min && policy->has_scaling_max)
        {
            printf("    Policy   : %.1f - %.1f MHz\n", (double)policy->scaling_min_khz / 1000.0, (double)policy->scaling_max_khz / 1000.0);
        }
        else
        {
            printf("    Policy   : N/A\n");
        }

        if (policy->has_hardware_min && policy->has_hardware_max)
        {
            printf("    Hardware : %.1f - %.1f MHz\n", (double)policy->hardware_min_khz / 1000.0, (double)policy->hardware_max_khz / 1000.0);
        }
        else
        {
            printf("    Hardware : N/A\n");
        }
    }
}

static void print_network(const HardwareInfo *info)
{
    printf("\nNetwork:\n");

    if (info->network_count == 0)
    {
        printf("  N/A\n");
        return;
    }

    for (size_t i = 0; i < info->network_count; i++)
    {
        const NetworkInterfaceInfo *network = &info->network_interfaces[i];

        printf("  %s\n", network->name);
        printf("    State : %s\n", network->state[0] ? network->state : "N/A");
        printf("    MAC   : %s\n", network->address[0] ? network->address : "N/A");

        if (network->has_mtu)
        {
            printf("    MTU   : %lu\n", network->mtu);
        }
        else
        {
            printf("    MTU   : N/A\n");
        }
    }
}

static void print_leds(const HardwareInfo *info)
{
    printf("\nLEDs:\n");

    if (info->led_count == 0)
    {
        printf("  N/A\n");
        return;
    }

    for (size_t i = 0; i < info->led_count; i++)
    {
        const LedInfo *led = &info->leds[i];

        printf("  %s\n", led->name);

        if (led->has_brightness && led->has_max_brightness)
        {
            printf("    Brightness : %ld/%ld\n", led->brightness, led->max_brightness);
        }
        else
        {
            printf("    Brightness : N/A\n");
        }

        printf("    Trigger    : %s\n", led->trigger[0] ? led->trigger : "N/A");
    }
}

void hardware_info_print(const HardwareInfo *info)
{
    if (!info)
    {
        log_error("hardware_info is null");
        return;
    }

    printf("\n");
    printf("========== Hardware ==========\n");
    print_thermal(info);
    print_cpu_frequency(info);
    print_network(info);
    print_leds(info);
    printf("\n");
}



