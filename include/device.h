#ifndef DEVICE_H
#define DEVICE_H

#include <stddef.h>
#include <sys/types.h>

#define DEVICE_NAME_SIZE 64
#define DEVICE_PATH_SIZE 256
#define DEVICE_MAX_COUNT 16

typedef enum
{
    DEVICE_UART = 0,
    DEVICE_I2C,
    DEVICE_OLED,
    DEVICE_GPIO,
    DEVICE_LED
} DeviceType;

typedef enum
{
    DEVICE_CLOSED = 0,
    DEVICE_OPEN,
    DEVICE_ERROR,
    DEVICE_DISABLED
} DeviceState;

typedef struct
{
    char name[DEVICE_NAME_SIZE];
    DeviceType type;
    char path[DEVICE_PATH_SIZE];
    int enabled;
    int baudrate;
    unsigned int address;
    unsigned int offset;
    int gpio_output;
    int initial_value;
    int bias;
    char led_name[DEVICE_NAME_SIZE];
} DeviceConfig;

typedef enum
{
    DEVICE_CONTROL_UART_WAIT = 1,
    DEVICE_CONTROL_UART_WRITE_ALL,
    DEVICE_CONTROL_UART_FLUSH,
    DEVICE_CONTROL_OLED_CLEAR,
    DEVICE_CONTROL_OLED_TEXT,
    DEVICE_CONTROL_OLED_REFRESH,
    DEVICE_CONTROL_OLED_ON,
    DEVICE_CONTROL_OLED_OFF,
    DEVICE_CONTROL_LED_ON,
    DEVICE_CONTROL_LED_OFF,
    DEVICE_CONTROL_LED_TRIGGER
} DeviceControl;

typedef struct
{
    const void *data;
    size_t size;
    int timeout_ms;
} DeviceWriteRequest;

typedef struct
{
    int x;
    int y;
    const char *text;
} DeviceText;

typedef struct Device Device;

typedef struct
{
    int (*open)(Device *device);
    ssize_t (*read)(Device *device, void *buffer, size_t size);
    ssize_t (*write)(Device *device, const void *buffer, size_t size);
    int (*control)(Device *device, DeviceControl request, void *argument);
    int (*close)(Device *device);
    void (*destroy)(Device *device);
} DeviceOps;

struct Device
{
    DeviceConfig config;
    const DeviceOps *ops;
    void *context;
    DeviceState state;
    int opened;
    int last_errno;
};

void device_init(Device *device, const DeviceConfig *config,
                 const DeviceOps *ops, void *context);
int device_open(Device *device);
ssize_t device_read(Device *device, void *buffer, size_t size);
ssize_t device_write(Device *device, const void *buffer, size_t size);
int device_control(Device *device, DeviceControl request, void *argument);
int device_close(Device *device);
void device_destroy(Device *device);
const char *device_type_string(DeviceType type);
const char *device_state_string(DeviceState state);

#endif
