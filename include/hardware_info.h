#ifndef HARDWARE_INFO_H
#define HARDWARE_INFO_H

#include <stddef.h>

#define HARDWARE_MAX_THERMAL_ZONES 16
#define HARDWARE_MAX_CPU_POLICIES 16
#define HARDWARE_MAX_NETWORK_INTERFACES 16
#define HARDWARE_MAX_LEDS 16

typedef struct
{
    char name[64];
    char type[64];
    double temperature;
    int has_temperature;
} ThermalZoneInfo;

typedef struct
{
    char name[32];
    char affected_cpus[64];
    char driver[64];
    char governor[64];
    unsigned long current_khz;
    unsigned long scaling_min_khz;
    unsigned long scaling_max_khz;
    unsigned long hardware_min_khz;
    unsigned long hardware_max_khz;
    int has_current;
    int has_scaling_min;
    int has_scaling_max;
    int has_hardware_min;
    int has_hardware_max;
} CpuFreqPolicyInfo;

typedef struct
{
    char name[64];
    char state[32];
    char address[64];
    unsigned long mtu;
    int has_mtu;
} NetworkInterfaceInfo;

typedef struct
{
    char name[64];
    long brightness;
    long max_brightness;
    char trigger[64];
    int has_brightness;
    int has_max_brightness;
} LedInfo;

typedef struct
{
    ThermalZoneInfo thermal_zones[HARDWARE_MAX_THERMAL_ZONES];
    size_t thermal_count;

    CpuFreqPolicyInfo cpu_policies[HARDWARE_MAX_CPU_POLICIES];
    size_t cpu_policy_count;

    NetworkInterfaceInfo network_interfaces[HARDWARE_MAX_NETWORK_INTERFACES];
    size_t network_count;

    LedInfo leds[HARDWARE_MAX_LEDS];
    size_t led_count;
} HardwareInfo;

int hardware_info_collect(HardwareInfo *info);
void hardware_info_print(const HardwareInfo *info);

#endif
