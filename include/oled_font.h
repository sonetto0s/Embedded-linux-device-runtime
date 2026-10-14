#ifndef OLED_FONT_H
#define OLED_FONT_H

#include <stdint.h>

const uint8_t *oled_font_ascii8x16(unsigned char ch);
const uint8_t *oled_font_zh16(uint32_t codepoint);

#endif

