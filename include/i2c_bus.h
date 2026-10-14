#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    int fd;
    uint8_t address;
} I2cBus;

void i2c_bus_init(I2cBus *bus);
int i2c_bus_open(I2cBus *bus, const char *device_path, uint8_t address);
int i2c_bus_probe(I2cBus *bus);
int i2c_bus_write(I2cBus *bus, const void *data, size_t size);
int i2c_bus_read(I2cBus *bus, void *data, size_t size);
void i2c_bus_close(I2cBus *bus);

#endif
