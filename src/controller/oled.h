/*
 * oled.h - 128x64 OLED (SSD1306 / SH1106, e.g. GME12864) over the remote I2C API
 *
 * The display has 8 pages (rows of 8 pixels). Text uses a 5x7 font
 * (A-Z, 0-9, space ! . - :), 6 px per character, lowercase shown as uppercase.
 */
#pragma once

#include "i2c1722.h"

#define OLED_BUS  0    /* I2C bus on the target */
#define OLED_ADDR 0x3C /* I2C address (some modules: 0x3D) */

#define OLED_WIDTH     128
#define OLED_PAGES     8
#define OLED_MAX_CHARS (OLED_WIDTH / 6) /* characters per line */

/**
 * Initializes the display and clears it.
 * @return 0, or -1 on error.
 */
int oled_init(struct i2c1722_link *link);

/**
 * Clears the whole display.
 * @return 0, or -1 on error.
 */
int oled_clear(struct i2c1722_link *link);

/**
 * Prints text on page (0..7) starting at column col (0..127).
 * Text beyond the right edge is cut off.
 * @return 0, or -1 on error.
 */
int oled_print(struct i2c1722_link *link, uint8_t page, uint8_t col, const char *text);

/**
 * Prints text horizontally centered on page (0..7), max. OLED_MAX_CHARS characters.
 * @return 0, or -1 on error.
 */
int oled_print_centered(struct i2c1722_link *link, uint8_t page, const char *text);
