#include "device_manager.h"
#include "test_framework.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    int opens;
    int closes;
    int destroys;
} ManagerFake;

static int manager_fake_open(Device *device)
{
    ((ManagerFake *)device->context)->opens++;
    return 0;
}

static int manager_fake_close(Device *device)
{
    ((ManagerFake *)device->context)->closes++;
    return 0;
}

static void manager_fake_destroy(Device *device)
{
    ((ManagerFake *)device->context)->destroys++;
}

static const DeviceOps manager_ops = {
    .open = manager_fake_open, .close = manager_fake_close, .destroy = manager_fake_destroy};

static DeviceConfig manager_config(const char *name)
{
    DeviceConfig config;
    device_config_init(&config);
    snprintf(config.name, sizeof(config.name), "%s", name);
    strcpy(config.path, "/dev/not-opened-by-registration");
    config.type = DEVICE_UART;
    return config;
}

void test_device_manager_registration_limits(void)
{
    DeviceManager manager;
    ManagerFake fakes[DEVICE_MAX_COUNT];
    memset(fakes, 0, sizeof(fakes));
    device_manager_init(&manager);
    TEST_ASSERT_EQ(manager.count, 0);
    for (size_t i = 0; i < DEVICE_MAX_COUNT; i++)
    {
        char name[DEVICE_NAME_SIZE];
        snprintf(name, sizeof(name), "device_%zu", i);
        DeviceConfig config = manager_config(name);
        TEST_ASSERT_EQ(device_manager_register(&manager, &config, &manager_ops, &fakes[i]), 0);
        TEST_ASSERT_EQ(fakes[i].opens, 0);
        TEST_ASSERT_NOT_NULL(device_manager_find(&manager, name));
    }
    TEST_ASSERT_EQ(manager.count, DEVICE_MAX_COUNT);
    DeviceConfig extra = manager_config("extra");
    errno = 0;
    TEST_ASSERT_EQ(device_manager_register(&manager, &extra, &manager_ops, NULL), -1);
    TEST_ASSERT_EQ(errno, ENOSPC);
    TEST_ASSERT_NULL(device_manager_find(&manager, "missing"));
    TEST_ASSERT_NULL(device_manager_get(&manager, DEVICE_MAX_COUNT));
    device_manager_destroy(&manager);
    TEST_ASSERT_EQ(manager.count, 0);
    for (size_t i = 0; i < DEVICE_MAX_COUNT; i++)
    {
        TEST_ASSERT_EQ(fakes[i].opens, 0);
        TEST_ASSERT_EQ(fakes[i].closes, 0);
        TEST_ASSERT_EQ(fakes[i].destroys, 1);
    }
}

void test_device_manager_duplicate_name(void)
{
    DeviceManager manager;
    ManagerFake fake = {0};
    device_manager_init(&manager);
    DeviceConfig config = manager_config("uart");
    TEST_ASSERT_EQ(device_manager_register(&manager, &config, &manager_ops, &fake), 0);
    errno = 0;
    TEST_ASSERT_EQ(device_manager_register(&manager, &config, &manager_ops, &fake), -1);
    TEST_ASSERT_EQ(errno, EEXIST);
    TEST_ASSERT_EQ(manager.count, 1);
    TEST_ASSERT_EQ(fake.opens, 0);
    device_manager_destroy(&manager);
    TEST_ASSERT_EQ(fake.destroys, 1);
}

void test_device_manager_close_all(void)
{
    DeviceManager manager;
    ManagerFake fakes[3] = {{0}};
    device_manager_init(&manager);
    for (size_t i = 0; i < 3; i++)
    {
        char name[DEVICE_NAME_SIZE];
        snprintf(name, sizeof(name), "device_%zu", i);
        DeviceConfig config = manager_config(name);
        TEST_ASSERT_EQ(device_manager_register(&manager, &config, &manager_ops, &fakes[i]), 0);
    }
    Device *first = device_manager_get(&manager, 0);
    Device *second = device_manager_get(&manager, 1);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    if (!first || !second)
    {
        device_manager_destroy(&manager);
        return;
    }
    TEST_ASSERT_EQ(device_open(first), 0);
    TEST_ASSERT_EQ(device_open(second), 0);
    second->state = DEVICE_ERROR;
    second->last_errno = EIO;
    TEST_ASSERT_EQ(device_manager_close_all(&manager), 0);
    TEST_ASSERT_EQ(device_manager_close_all(&manager), 0);
    TEST_ASSERT_EQ(fakes[0].closes, 1);
    TEST_ASSERT_EQ(fakes[1].closes, 1);
    TEST_ASSERT_EQ(fakes[2].closes, 0);
    TEST_ASSERT_EQ(first->opened, 0);
    TEST_ASSERT_EQ(second->opened, 0);
    device_manager_destroy(&manager);
    TEST_ASSERT_EQ(fakes[0].closes, 1);
    TEST_ASSERT_EQ(fakes[1].closes, 1);
}

void test_device_manager_load_transaction(void)
{
    const char *dir = getenv("MINISHELL_TEST_DIR");
    if (!dir || !*dir)
    {
        dir = "/tmp";
    }
    char path[512];
    int length = snprintf(path, sizeof(path), "%s/device-manager-%ld.conf", dir, (long)getpid());
    TEST_ASSERT(length > 0 && (size_t)length < sizeof(path));
    if (length <= 0 || (size_t)length >= sizeof(path))
    {
        return;
    }
    FILE *fp = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    if (!fp)
    {
        return;
    }
    fputs("[replacement]\ntype=uart\npath=/dev/not-opened-at-load\n", fp);
    fclose(fp);
    DeviceManager manager;
    ManagerFake fake = {0};
    device_manager_init(&manager);
    DeviceConfig config = manager_config("original");
    TEST_ASSERT_EQ(device_manager_register(&manager, &config, &manager_ops, &fake), 0);
    Device *original = device_manager_find(&manager, "original");
    TEST_ASSERT_NOT_NULL(original);
    if (!original)
    {
        device_manager_destroy(&manager);
        unlink(path);
        return;
    }
    TEST_ASSERT_EQ(device_open(original), 0);
    errno = 0;
    TEST_ASSERT_EQ(device_manager_load(&manager, path), -1);
    TEST_ASSERT_EQ(errno, EBUSY);
    TEST_ASSERT(device_manager_find(&manager, "original") == original);
    TEST_ASSERT_EQ(original->opened, 1);
    TEST_ASSERT_EQ(fake.destroys, 0);
    TEST_ASSERT_EQ(device_close(original), 0);
    TEST_ASSERT_EQ(device_manager_load(&manager, path), 0);
    TEST_ASSERT_EQ(fake.destroys, 1);
    TEST_ASSERT_NULL(device_manager_find(&manager, "original"));
    Device *replacement = device_manager_find(&manager, "replacement");
    TEST_ASSERT_NOT_NULL(replacement);
    if (replacement)
    {
        TEST_ASSERT_EQ(replacement->opened, 0);
        TEST_ASSERT_EQ(replacement->state, DEVICE_CLOSED);
    }
    fp = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    if (fp)
    {
        fputs("[bad]\ntype=gpio\npath=/dev/gpiochip0\noffset=-1\n", fp);
        fclose(fp);
        TEST_ASSERT_EQ(device_manager_load(&manager, path), -1);
        TEST_ASSERT(device_manager_find(&manager, "replacement") == replacement);
        TEST_ASSERT_EQ(manager.count, 1);
    }
    device_manager_destroy(&manager);
    unlink(path);
}
