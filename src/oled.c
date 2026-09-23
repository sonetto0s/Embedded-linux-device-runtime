#include "oled.h"
#include "oled_font.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int oled_write_commands(Oled *oled, const uint8_t *commands, size_t count)
{
    if (!oled || !oled->bus || (!commands && count > 0))
    {
        errno = EINVAL;
        return -1;
    }

    uint8_t packet[33];

    while (count > 0)
    {
        size_t chunk = count;

        if (chunk > sizeof(packet) - 1)
        {
            chunk = sizeof(packet) - 1;
        }

        packet[0] = 0x00;
        memcpy(packet + 1, commands, chunk);

        if (i2c_bus_write(oled->bus, packet, chunk + 1) < 0)
        {
            return -1;
        }

        commands += chunk;
        count -= chunk;
    }

    return 0;
}

static int oled_write_data(Oled *oled, const uint8_t *data, size_t count)
{
    if (!oled || !oled->bus || (!data && count > 0))
    {
        errno = EINVAL;
        return -1;
    }

    uint8_t packet[17];

    while (count > 0)
    {
        size_t chunk = count;

        if (chunk > sizeof(packet) - 1)
        {
            chunk = sizeof(packet) - 1;
        }

        packet[0] = 0x40;
        memcpy(packet + 1, data, chunk);

        if (i2c_bus_write(oled->bus, packet, chunk + 1) < 0)
        {
            return -1;
        }

        data += chunk;
        count -= chunk;
    }

    return 0;
}

static void oled_draw_missing_zh16(Oled *oled, int x, int y)
{
    for (int row = 0; row < 16; row++)
    {
        for (int col = 0; col < 16; col++)
        {
            int border = row == 0 || row == 15 || col == 0 || col == 15;
            int cross = row == col || row + col == 15;

            oled_set_pixel(oled, x + col, y + row, border || cross);
        }
    }
}

static int utf8_next(const char **text, uint32_t *codepoint)
{
    if (!text || !*text || !codepoint)
    {
        errno = EINVAL;
        return -1;
    }

    const unsigned char *s = (const unsigned char *)*text;

    if (s[0] == '\0')
    {
        return 0;
    }

    if (s[0] < 0x80)
    {
        *codepoint = s[0];
        *text += 1;
        return 1;
    }

    if ((s[0] & 0xE0) == 0xC0)
    {
        if (s[1] == '\0' || (s[1] & 0xC0) != 0x80)
        {
            errno = EILSEQ;
            return -1;
        }

        uint32_t cp = ((uint32_t)(s[0] & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);

        if (cp < 0x80)
        {
            errno = EILSEQ;
            return -1;
        }

        *codepoint = cp;
        *text += 2;
        return 1;
    }

    if ((s[0] & 0xF0) == 0xE0)
    {
        if (s[1] == '\0' || s[2] == '\0' || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80)
        {
            errno = EILSEQ;
            return -1;
        }

        uint32_t cp = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (uint32_t)(s[2] & 0x3F);

        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))
        {
            errno = EILSEQ;
            return -1;
        }

        *codepoint = cp;
        *text += 3;
        return 1;
    }

    if ((s[0] & 0xF8) == 0xF0)
    {
        if (s[1] == '\0' || s[2] == '\0' || s[3] == '\0' || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80)
        {
            errno = EILSEQ;
            return -1;
        }

        uint32_t cp = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) | ((uint32_t)(s[2] & 0x3F) << 6) | (uint32_t)(s[3] & 0x3F);

        if (cp < 0x10000 || cp > 0x10FFFF)
        {
            errno = EILSEQ;
            return -1;
        }

        *codepoint = cp;
        *text += 4;
        return 1;
    }

    errno = EILSEQ;
    return -1;
}

int oled_init(Oled *oled, I2cBus *bus)
{
    if (!oled || !bus || bus->fd < 0)
    {
        errno = EINVAL;
        return -1;
    }

    oled->bus = bus;
    memset(oled->buffer, 0, sizeof(oled->buffer));

    static const uint8_t commands[] =
    {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0x7F,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0x2E,
        0xAF
    };

    if (oled_write_commands(oled, commands, sizeof(commands)) < 0)
    {
        return -1;
    }

    return oled_refresh(oled);
}

void oled_clear(Oled *oled)
{
    if (!oled)
    {
        return;
    }

    memset(oled->buffer, 0, sizeof(oled->buffer));
}

void oled_fill(Oled *oled, uint8_t value)
{
    if (!oled)
    {
        return;
    }

    memset(oled->buffer, value, sizeof(oled->buffer));
}

void oled_set_pixel(Oled *oled, int x, int y, int enabled)
{
    if (!oled)
    {
        return;
    }

    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
    {
        return;
    }

    size_t index = (size_t)x + ((size_t)y / 8U) * OLED_WIDTH;
    uint8_t mask = (uint8_t)(1U << ((unsigned int)y & 7U));

    if (enabled)
    {
        oled->buffer[index] |= mask;
    }
    else
    {
        oled->buffer[index] &= (uint8_t)~mask;
    }
}

void oled_draw_ascii16(Oled *oled, int x, int y, char ch)
{
    if (!oled)
    {
        return;
    }

    const uint8_t *glyph = oled_font_ascii8x16((unsigned char)ch);

    for (int row = 0; row < 16; row++)
    {
        uint8_t bits = glyph[row];

        for (int col = 0; col < 8; col++)
        {
            int enabled = (bits & (0x80U >> col)) != 0;

            oled_set_pixel(oled, x + col, y + row, enabled);
        }
    }
}

void oled_draw_zh16(Oled *oled, int x, int y, uint32_t codepoint)
{
    if (!oled)
    {
        return;
    }

    const uint8_t *glyph = oled_font_zh16(codepoint);

    if (!glyph)
    {
        oled_draw_missing_zh16(oled, x, y);
        return;
    }

    for (int row = 0; row < 16; row++)
    {
        uint16_t bits = ((uint16_t)glyph[row * 2] << 8) | glyph[row * 2 + 1];

        for (int col = 0; col < 16; col++)
        {
            int enabled = (bits & (0x8000U >> col)) != 0;

            oled_set_pixel(oled, x + col, y + row, enabled);
        }
    }
}

int oled_draw_text_utf8(Oled *oled, int x, int y, const char *text)
{
    if (!oled || !text || x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
    {
        errno = EINVAL;
        return -1;
    }

    int cursor_x = x;
    int cursor_y = y;

    while (*text)
    {
        uint32_t codepoint;

        if (utf8_next(&text, &codepoint) < 0)
        {
            return -1;
        }

        if (codepoint == '\n')
        {
            cursor_x = x;
            cursor_y += 16;
            continue;
        }

        int width = codepoint < 0x80 ? 8 : 16;

        if (cursor_x + width > OLED_WIDTH)
        {
            cursor_x = x;
            cursor_y += 16;
        }

        if (cursor_y + 16 > OLED_HEIGHT)
        {
            break;
        }

        if (codepoint < 0x80)
        {
            oled_draw_ascii16(oled, cursor_x, cursor_y, (char)codepoint);
        }
        else
        {
            oled_draw_zh16(oled, cursor_x, cursor_y, codepoint);
        }

        cursor_x += width;
    }

    return 0;
}

int oled_refresh(Oled *oled)
{
    if (!oled || !oled->bus)
    {
        errno = EINVAL;
        return -1;
    }

    static const uint8_t commands[] =
    {
        0x21, 0x00, 0x7F,
        0x22, 0x00, 0x07
    };

    if (oled_write_commands(oled, commands, sizeof(commands)) < 0)
    {
        return -1;
    }

    return oled_write_data(oled, oled->buffer, sizeof(oled->buffer));
}

int oled_display_on(Oled *oled)
{
    static const uint8_t command = 0xAF;

    return oled_write_commands(oled, &command, 1);
}

int oled_display_off(Oled *oled)
{
    static const uint8_t command = 0xAE;

    return oled_write_commands(oled, &command, 1);
}


