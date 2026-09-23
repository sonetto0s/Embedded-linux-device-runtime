#include "i2c_bus.h"
#include "oled.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int parse_address(const char *text, uint8_t *address)
{
    if (!text || !address)
    {
        return -1;
    }

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);

    if (errno != 0 || !end || *end != '\0' || value < 0x03 || value > 0x77)
    {
        return -1;
    }

    *address = (uint8_t)value;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <i2c-device> <address>\n", argv[0]);
        fprintf(stderr, "Example: %s /dev/i2c-4 0x3c\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint8_t address;

    if (parse_address(argv[2], &address) < 0)
    {
        fprintf(stderr, "Invalid I2C address: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    I2cBus bus;
    i2c_bus_init(&bus);

    if (i2c_bus_open(&bus, argv[1], address) < 0)
    {
        perror("i2c_bus_open");
        return EXIT_FAILURE;
    }

    Oled oled;

    if (oled_init(&oled, &bus) < 0)
    {
        perror("oled_init");
        i2c_bus_close(&bus);
        return EXIT_FAILURE;
    }

    oled_clear(&oled);

    if (oled_draw_text_utf8(&oled, 0, 0, "香橙派 OLED") < 0 || oled_draw_text_utf8(&oled, 0, 16, "系统正常") < 0 || oled_draw_text_utf8(&oled, 0, 32, "温度: 46.2 C") < 0 || oled_draw_text_utf8(&oled, 0, 48, "网络: OK") < 0)
    {
        perror("oled_draw_text_utf8");
        i2c_bus_close(&bus);
        return EXIT_FAILURE;
    }

    if (oled_refresh(&oled) < 0)
    {
        perror("oled_refresh");
        i2c_bus_close(&bus);
        return EXIT_FAILURE;
    }

    printf("OLED UTF-8 demo displayed: bus=%s address=0x%02X\n", argv[1], address);

    i2c_bus_close(&bus);
    return EXIT_SUCCESS;
}


