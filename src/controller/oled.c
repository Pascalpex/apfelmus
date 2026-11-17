/*
 * oled.c - 128x64 OLED (SSD1306 / SH1106, e.g. GME12864) over the remote I2C API
 *
 * Every I2C write starts with a control byte:
 *   0x00 = the following bytes are commands
 *   0x40 = the following bytes are display data (one byte = 8 vertical pixels)
 */
#include <ctype.h>
#include <string.h>

#include "oled.h"

#define OLED_CHUNK 32 /* data bytes per I2C transaction */

/* Control byte 0x00 + commands, one transaction */
static int oled_cmds(struct i2c1722_link *link, const uint8_t *cmds, size_t n)
{
    uint8_t buf[I2C1722_MAX_MSGS];
    buf[0] = 0x00;
    memcpy(&buf[1], cmds, n);
    return i2c1722_write(link, OLED_BUS, OLED_ADDR, buf, n + 1);
}

/* Control byte 0x40 + display data, split into chunks (column auto-increments) */
static int oled_data(struct i2c1722_link *link, const uint8_t *data, size_t n)
{
    uint8_t buf[OLED_CHUNK + 1];
    buf[0] = 0x40;
    while (n > 0) {
        size_t c = n < OLED_CHUNK ? n : OLED_CHUNK;
        memcpy(&buf[1], data, c);
        if (i2c1722_write(link, OLED_BUS, OLED_ADDR, buf, c + 1) != 0) {
            return -1;
        }
        data += c;
        n -= c;
    }
    return 0;
}

/* Page addressing: works on SSD1306 and SH1106 */
static int oled_set_pos(struct i2c1722_link *link, uint8_t page, uint8_t col)
{
    const uint8_t cmds[] = {(uint8_t)(0xB0 | page), (uint8_t)(col & 0x0F), (uint8_t)(0x10 | (col >> 4))};
    return oled_cmds(link, cmds, sizeof(cmds));
}

/* ---------------- 5x7 font ---------------- */

/* Glyphs: column-major, LSB = top row */
static const uint8_t font_digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},
};

static const uint8_t font_upper[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22}, /* A B C */
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, /* D E F */
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, /* G H I */
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40}, /* J K L */
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E}, /* M N O */
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, /* P Q R */
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, /* S T U */
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63}, /* V W X */
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},                             /* Y Z */
};

/* Returns the glyph for c; lowercase is shown as uppercase, unknown characters as space. */
static const uint8_t *glyph(char c)
{
    static const uint8_t sp[5]    = {0x00,0x00,0x00,0x00,0x00};
    static const uint8_t ex[5]    = {0x00,0x00,0x5F,0x00,0x00};
    static const uint8_t dot[5]   = {0x00,0x60,0x60,0x00,0x00};
    static const uint8_t dash[5]  = {0x08,0x08,0x08,0x08,0x08};
    static const uint8_t colon[5] = {0x00,0x36,0x36,0x00,0x00};

    c = (char)toupper((unsigned char)c);
    if (c >= 'A' && c <= 'Z') {
        return font_upper[c - 'A'];
    }
    if (c >= '0' && c <= '9') {
        return font_digits[c - '0'];
    }
    switch (c) {
    case '!': return ex;
    case '.': return dot;
    case '-': return dash;
    case ':': return colon;
    default:  return sp;
    }
}

/* ---------------- public functions ---------------- */

int oled_init(struct i2c1722_link *link)
{
    static const uint8_t init[] = {
        0xAE,             /* display off */
        0xD5, 0x80,       /* clock div */
        0xA8, 0x3F,       /* multiplex 64 */
        0xD3, 0x00,       /* offset 0 */
        0x40,             /* start line 0 */
        0x8D, 0x14,       /* charge pump on */
        0x20, 0x02,       /* page addressing mode */
        0xA1, 0xC8,       /* flip so text is upright */
        0xDA, 0x12,       /* COM pins */
        0x81, 0xCF,       /* contrast */
        0xD9, 0xF1,       /* precharge */
        0xDB, 0x40,       /* VCOMH */
        0xA4, 0xA6,       /* show RAM, normal (not inverted) */
        0xAF,             /* display on */
    };

    if (oled_cmds(link, init, sizeof(init)) != 0) {
        return -1;
    }
    return oled_clear(link);
}

int oled_clear(struct i2c1722_link *link)
{
    static const uint8_t zeros[132] = {0}; /* 132: also clears the SH1106 border columns */

    for (uint8_t p = 0; p < OLED_PAGES; p++) {
        if (oled_set_pos(link, p, 0) != 0 || oled_data(link, zeros, sizeof(zeros)) != 0) {
            return -1;
        }
    }
    return 0;
}

int oled_print(struct i2c1722_link *link, uint8_t page, uint8_t col, const char *text)
{
    uint8_t buf[OLED_WIDTH];
    size_t n = 0;

    for (; *text && col + n + 6 <= OLED_WIDTH; text++) {
        memcpy(&buf[n], glyph(*text), 5); /* 5 columns + 1 blank spacing */
        buf[n + 5] = 0;
        n += 6;
    }
    if (oled_set_pos(link, page, col) != 0) {
        return -1;
    }
    return oled_data(link, buf, n);
}

int oled_print_centered(struct i2c1722_link *link, uint8_t page, const char *text)
{
    size_t len = strlen(text);
    if (len > OLED_MAX_CHARS) {
        len = OLED_MAX_CHARS;
    }
    return oled_print(link, page, (uint8_t)((OLED_WIDTH - 6 * len) / 2), text);
}
