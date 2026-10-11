#include "device_backend.h"
#include "device_config.h"
#include "test_framework.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int backend_fd_count(void)
{
    DIR *dir = opendir("/proc/self/fd");
    if (!dir)
    {
        return -1;
    }
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            count++;
        }
    }
    closedir(dir);
    return count;
}

void test_device_backend_missing_hardware_no_leak(void)
{
    int before = backend_fd_count();
    TEST_ASSERT(before >= 0);
    if (before < 0)
    {
        return;
    }
    const DeviceType types[] = {DEVICE_UART, DEVICE_I2C, DEVICE_OLED, DEVICE_GPIO, DEVICE_LED};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        for (int round = 0; round < 8; round++)
        {
            DeviceConfig config;
            device_config_init(&config);
            snprintf(config.name, sizeof(config.name), "missing_%zu", i);
            strcpy(config.path, "/__minishell_missing_device__/node");
            strcpy(config.led_name, "__minishell_missing_led__");
            config.type = types[i];
            config.address = 0x3c;
            Device device;
            int ret = device_backend_init(&device, &config);
            TEST_ASSERT_EQ(ret, 0);
            if (ret < 0)
            {
                continue;
            }
            TEST_ASSERT_EQ(device.opened, 0);
            TEST_ASSERT_EQ(device_open(&device), -1);
            TEST_ASSERT_EQ(device.opened, 0);
            TEST_ASSERT_EQ(device_close(&device), 0);
            device_destroy(&device);
        }
    }
    TEST_ASSERT_EQ(backend_fd_count(), before);
}

void test_device_backend_rejects_wrong_node(void)
{
    const DeviceType types[] = {DEVICE_UART, DEVICE_I2C, DEVICE_OLED, DEVICE_GPIO};
    int before = backend_fd_count();
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        DeviceConfig config;
        device_config_init(&config);
        snprintf(config.name, sizeof(config.name), "wrong_%zu", i);
        strcpy(config.path, "/dev/null");
        config.type = types[i];
        config.address = 0x3c;
        Device device;
        int ret = device_backend_init(&device, &config);
        TEST_ASSERT_EQ(ret, 0);
        if (ret < 0)
        {
            continue;
        }
        TEST_ASSERT_EQ(device_open(&device), -1);
        TEST_ASSERT_EQ(device.opened, 0);
        device_destroy(&device);
    }
    TEST_ASSERT_EQ(backend_fd_count(), before);
}
