#ifndef OLED_H
#define OLED_H

#include "i2c_bus.h"

#include <stdint.h>

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_BUFFER_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

typedef struct
{
    I2cBus *bus;
    uint8_t buffer[OLED_BUFFER_SIZE];
} Oled;

int oled_init(Oled *oled, I2cBus *bus);

void oled_clear(Oled *oled);
void oled_fill(Oled *oled, uint8_t value);

void oled_set_pixel(Oled *oled, int x, int y, int enabled);

void oled_draw_ascii16(Oled *oled, int x, int y, char ch);
void oled_draw_zh16(Oled *oled, int x, int y, uint32_t codepoint);

int oled_draw_text_utf8(Oled *oled, int x, int y, const char *text);

int oled_refresh(Oled *oled);

int oled_display_on(Oled *oled);
int oled_display_off(Oled *oled);

#endif


