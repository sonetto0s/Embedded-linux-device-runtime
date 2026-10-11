#include "device.h"
#include "device_config.h"
#include "test_framework.h"
#include <errno.h>
#include <string.h>

typedef struct
{
    int opens;
    int closes;
    int destroys;
    int operation_errno;
    int close_errno;
    int owned;
} FakeDevice;

static int fake_open(Device *device)
{
    FakeDevice *fake = device->context;
    fake->opens++;
    fake->owned = 1;
    return 0;
}

static ssize_t fake_read(Device *device, void *buffer, size_t size)
{
    FakeDevice *fake = device->context;
    if (fake->operation_errno)
    {
        errno = fake->operation_errno;
        return -1;
    }
    if (size > 0)
    {
        ((unsigned char *)buffer)[0] = 0x5a;
    }
    return size > 0 ? 1 : 0;
}

static ssize_t fake_write(Device *device, const void *buffer, size_t size)
{
    FakeDevice *fake = device->context;
    (void)buffer;
    if (fake->operation_errno)
    {
        errno = fake->operation_errno;
        return -1;
    }
    return (ssize_t)size;
}

static int fake_control(Device *device, DeviceControl request, void *argument)
{
    FakeDevice *fake = device->context;
    (void)request;
    (void)argument;
    if (fake->operation_errno)
    {
        errno = fake->operation_errno;
        return -1;
    }
    return 0;
}

static int fake_close(Device *device)
{
    FakeDevice *fake = device->context;
    fake->closes++;
    fake->owned = 0;
    if (fake->close_errno)
    {
        errno = fake->close_errno;
        return -1;
    }
    return 0;
}

static void fake_destroy(Device *device)
{
    FakeDevice *fake = device->context;
    fake->destroys++;
}

static const DeviceOps fake_ops = {fake_open,    fake_read,  fake_write,
                                   fake_control, fake_close, fake_destroy};

static void make_device(Device *device, FakeDevice *fake)
{
    DeviceConfig config;
    device_config_init(&config);
    strcpy(config.name, "fake_uart");
    strcpy(config.path, "/dev/fake_uart");
    config.type = DEVICE_UART;
    memset(fake, 0, sizeof(*fake));
    device_init(device, &config, &fake_ops, fake);
}

void test_device_lazy_lifecycle(void)
{
    Device device;
    FakeDevice fake;
    unsigned char byte = 0;
    make_device(&device, &fake);
    TEST_ASSERT_EQ(fake.opens, 0);
    TEST_ASSERT_EQ(device.state, DEVICE_CLOSED);
    TEST_ASSERT_EQ(device.opened, 0);
    errno = 0;
    TEST_ASSERT_EQ(device_read(&device, &byte, 1), -1);
    TEST_ASSERT_EQ(errno, EBADF);
    TEST_ASSERT_EQ(device_open(&device), 0);
    TEST_ASSERT_EQ(device_open(&device), 0);
    TEST_ASSERT_EQ(fake.opens, 1);
    TEST_ASSERT_EQ(device.state, DEVICE_OPEN);
    TEST_ASSERT_EQ(device_read(&device, &byte, 1), 1);
    TEST_ASSERT_EQ(byte, 0x5a);
    TEST_ASSERT_EQ(device_write(&device, &byte, 1), 1);
    TEST_ASSERT_EQ(device_close(&device), 0);
    TEST_ASSERT_EQ(device_close(&device), 0);
    TEST_ASSERT_EQ(fake.closes, 1);
    TEST_ASSERT_EQ(fake.owned, 0);
    TEST_ASSERT_EQ(device.opened, 0);
    TEST_ASSERT_EQ(device.state, DEVICE_CLOSED);
    device_destroy(&device);
    TEST_ASSERT_EQ(fake.destroys, 1);
    TEST_ASSERT_EQ(fake.closes, 1);
}

void test_device_disabled_no_side_effect(void)
{
    Device device;
    FakeDevice fake;
    make_device(&device, &fake);
    device.config.enabled = 0;
    errno = 0;
    TEST_ASSERT_EQ(device_open(&device), -1);
    TEST_ASSERT_EQ(errno, EACCES);
    TEST_ASSERT_EQ(fake.opens, 0);
    TEST_ASSERT_EQ(device.opened, 0);
    TEST_ASSERT_EQ(device_close(&device), 0);
    TEST_ASSERT_EQ(fake.closes, 0);
    device_destroy(&device);
}

void test_device_transient_errors_recover(void)
{
    Device device;
    FakeDevice fake;
    unsigned char byte = 0;
    const int errors[] = {EAGAIN, ETIMEDOUT, EINVAL, EPROTO, EMSGSIZE};
    make_device(&device, &fake);
    TEST_ASSERT_EQ(device_open(&device), 0);
    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); i++)
    {
        fake.operation_errno = errors[i];
        errno = 0;
        TEST_ASSERT_EQ(device_read(&device, &byte, 1), -1);
        TEST_ASSERT_EQ(errno, errors[i]);
        TEST_ASSERT_EQ(device.state, DEVICE_OPEN);
        TEST_ASSERT_EQ(device.opened, 1);
        fake.operation_errno = 0;
        TEST_ASSERT_EQ(device_read(&device, &byte, 1), 1);
    }
    device_destroy(&device);
    TEST_ASSERT_EQ(fake.closes, 1);
}

void test_device_error_can_release_and_reopen(void)
{
    Device device;
    FakeDevice fake;
    unsigned char byte = 0;
    make_device(&device, &fake);
    TEST_ASSERT_EQ(device_open(&device), 0);
    fake.operation_errno = EIO;
    TEST_ASSERT_EQ(device_write(&device, &byte, 1), -1);
    TEST_ASSERT_EQ(device.state, DEVICE_ERROR);
    TEST_ASSERT_EQ(device.last_errno, EIO);
    TEST_ASSERT_EQ(device.opened, 1);
    TEST_ASSERT_EQ(fake.owned, 1);
    fake.operation_errno = 0;
    TEST_ASSERT_EQ(device_read(&device, &byte, 1), -1);
    TEST_ASSERT_EQ(errno, EIO);
    TEST_ASSERT_EQ(device.state, DEVICE_ERROR);
    TEST_ASSERT_EQ(fake.owned, 1);
    errno = 0;
    TEST_ASSERT_EQ(device_open(&device), -1);
    TEST_ASSERT_EQ(errno, EBUSY);
    TEST_ASSERT_EQ(fake.opens, 1);
    TEST_ASSERT_EQ(device_close(&device), 0);
    TEST_ASSERT_EQ(fake.owned, 0);
    TEST_ASSERT_EQ(device_open(&device), 0);
    TEST_ASSERT_EQ(device_read(&device, &byte, 1), 1);
    device_destroy(&device);
    TEST_ASSERT_EQ(fake.opens, 2);
    TEST_ASSERT_EQ(fake.closes, 2);
}

void test_device_unsupported_operation(void)
{
    Device device;
    FakeDevice fake;
    DeviceOps ops = fake_ops;
    unsigned char byte = 0;
    make_device(&device, &fake);
    ops.read = NULL;
    ops.write = NULL;
    ops.control = NULL;
    device.ops = &ops;
    TEST_ASSERT_EQ(device_open(&device), 0);
    errno = 0;
    TEST_ASSERT_EQ(device_read(&device, &byte, 1), -1);
    TEST_ASSERT_EQ(errno, ENOTSUP);
    TEST_ASSERT_EQ(device_write(&device, &byte, 1), -1);
    TEST_ASSERT_EQ(errno, ENOTSUP);
    TEST_ASSERT_EQ(device_control(&device, DEVICE_CONTROL_LED_ON, NULL), -1);
    TEST_ASSERT_EQ(errno, ENOTSUP);
    TEST_ASSERT_EQ(device.state, DEVICE_OPEN);
    device_destroy(&device);
}

void test_device_invalid_arguments(void)
{
    Device device;
    FakeDevice fake;
    make_device(&device, &fake);
    TEST_ASSERT_EQ(device_open(NULL), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(device_open(&device), 0);
    TEST_ASSERT_EQ(device_read(&device, NULL, 1), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(device_write(&device, NULL, 1), -1);
    TEST_ASSERT_EQ(errno, EINVAL);
    TEST_ASSERT_EQ(fake.closes, 0);
    TEST_ASSERT_EQ(device.state, DEVICE_OPEN);
    device_destroy(&device);
}
