#include "device_config.h"
#include "test_framework.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int config_fixture(char *path, size_t size, const char *text)
{
    static unsigned int sequence;
    const char *dir = getenv("MINISHELL_TEST_DIR");
    if (!dir || !*dir)
    {
        dir = "/tmp";
    }
    int length =
        snprintf(path, size, "%s/device-config-%ld-%u.conf", dir, (long)getpid(), sequence++);
    if (length < 0 || (size_t)length >= size)
    {
        return -1;
    }
    FILE *fp = fopen(path, "w");
    if (!fp)
    {
        return -1;
    }
    int ret = fputs(text, fp) < 0 ? -1 : 0;
    if (fclose(fp) != 0)
    {
        ret = -1;
    }
    return ret;
}

void test_device_config_all_types(void)
{
    const char *text =
        "# Board settings\n[uart]\ntype=uart\npath=/dev/ttyS3\n"
        "[bus]\ntype=i2c\npath=/dev/i2c-4\naddress=0x3c\n"
        "[screen]\ntype=oled\npath=/dev/i2c-4\naddress=60\n"
        "[button]\ntype=gpio\npath=/dev/gpiochip4\noffset=6\nbias=pull_up\n"
        "[output]\ntype=gpio\npath=/dev/gpiochip1\noffset=2\ndirection=output\ninitial_value=1\n"
        "[indicator]\ntype=led\nled_name=blue_led\nenabled=0\n";
    char path[512];
    int ret = config_fixture(path, sizeof(path), text);
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        return;
    }
    DeviceConfigList list;
    memset(&list, 0, sizeof(list));
    ret = device_config_load(&list, path);
    TEST_ASSERT_EQ(ret, 0);
    if (ret == 0)
    {
        TEST_ASSERT_EQ(list.count, 6);
        TEST_ASSERT_EQ(list.devices[0].type, DEVICE_UART);
        TEST_ASSERT_EQ(list.devices[0].baudrate, 115200);
        TEST_ASSERT_EQ(list.devices[0].enabled, 1);
        TEST_ASSERT_EQ(list.devices[1].address, 0x3c);
        TEST_ASSERT_EQ(list.devices[2].type, DEVICE_OLED);
        TEST_ASSERT_EQ(list.devices[2].address, 60);
        TEST_ASSERT_EQ(list.devices[3].gpio_output, 0);
        TEST_ASSERT_EQ(list.devices[3].offset, 6);
        TEST_ASSERT_EQ(list.devices[4].gpio_output, 1);
        TEST_ASSERT_EQ(list.devices[4].initial_value, 1);
        TEST_ASSERT_EQ(list.devices[5].type, DEVICE_LED);
        TEST_ASSERT_EQ(list.devices[5].enabled, 0);
        TEST_ASSERT_STR_EQ(list.devices[5].led_name, "blue_led");
    }
    unlink(path);
}

void test_device_config_rejects_and_preserves(void)
{
    const char *bad[] = {
        "[x]\ntype=unknown\npath=/dev/x\n",
        "[x]\ntype=uart\n",
        "[x]\ntype=uart\npath=relative\n",
        "[x]\ntype=uart\npath=/dev/x\nbaudrate=12345\n",
        "[x]\ntype=uart\npath=/dev/x\nenabled=2\n",
        "[x]\ntype=i2c\npath=/dev/x\n",
        "[x]\ntype=i2c\npath=/dev/x\naddress=0x78\n",
        "[x]\ntype=gpio\npath=/dev/x\n",
        "[x]\ntype=gpio\npath=/dev/x\noffset=-1\n",
        "[x]\ntype=gpio\npath=/dev/x\noffset=0\ndirection=input\ninitial_value=1\n",
        "[x]\ntype=gpio\npath=/dev/x\noffset=0\ndirection=output\nbias=pull_up\n",
        "[x]\ntype=led\nled_name=../bad\n",
        "[x]\ntype=uart\npath=/dev/x\naddress=60\n",
        "[x]\ntype=uart\npath=/dev/x\nunknown=1\n",
        "[x]\ntype=uart\npath=/dev/x\npath=/dev/y\n",
        "[x]\ntype=uart\npath=/dev/x\n[x]\ntype=led\nled_name=blue_led\n",
        "type=uart\npath=/dev/x\n",
        "[x\ntype=uart\npath=/dev/x\n"};
    DeviceConfigList list;
    memset(&list, 0, sizeof(list));
    list.count = 1;
    device_config_init(&list.devices[0]);
    strcpy(list.devices[0].name, "stable");
    strcpy(list.devices[0].path, "/dev/stable");
    DeviceConfigList original = list;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        char path[512];
        int ret = config_fixture(path, sizeof(path), bad[i]);
        TEST_ASSERT_EQ(ret, 0);
        if (ret < 0)
        {
            continue;
        }
        TEST_ASSERT_EQ(device_config_load(&list, path), -1);
        TEST_ASSERT_EQ(memcmp(&list, &original, sizeof(list)), 0);
        unlink(path);
    }
}

void test_device_config_limits(void)
{
    char text[4096];
    size_t used = 0;
    for (int i = 0; i < DEVICE_MAX_COUNT + 1; i++)
    {
        int n = snprintf(text + used, sizeof(text) - used,
                         "[uart%d]\ntype=uart\npath=/dev/ttyS%d\n", i, i);
        if (n < 0 || (size_t)n >= sizeof(text) - used)
        {
            TEST_ASSERT(0);
            return;
        }
        used += (size_t)n;
    }
    char path[512];
    int ret = config_fixture(path, sizeof(path), text);
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        return;
    }
    DeviceConfigList list;
    memset(&list, 0, sizeof(list));
    TEST_ASSERT_EQ(device_config_load(&list, path), -1);
    TEST_ASSERT_EQ(list.count, 0);
    unlink(path);
    memset(text, 'x', 2048);
    text[2048] = '\n';
    text[2049] = '\0';
    ret = config_fixture(path, sizeof(path), text);
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        return;
    }
    TEST_ASSERT_EQ(device_config_load(&list, path), -1);
    TEST_ASSERT_EQ(list.count, 0);
    unlink(path);
}

void test_device_config_file_errors(void)
{
    DeviceConfigList list;
    memset(&list, 0, sizeof(list));
    TEST_ASSERT_EQ(device_config_load(NULL, "/tmp/not-used"), -1);
    TEST_ASSERT_EQ(device_config_load(&list, NULL), -1);
    char path[512];
    int ret = config_fixture(path, sizeof(path), "[stable]\ntype=uart\npath=/dev/stable\n");
    TEST_ASSERT_EQ(ret, 0);
    if (ret < 0)
    {
        return;
    }
    TEST_ASSERT_EQ(device_config_load(&list, path), 0);
    DeviceConfigList before = list;
    unlink(path);
    TEST_ASSERT_EQ(device_config_load(&list, path), -1);
    TEST_ASSERT_EQ(memcmp(&list, &before, sizeof(list)), 0);
}
