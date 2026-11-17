#ifndef DISPLAY_H
#define DISPLAY_H

#include "i2c1722.h"

#include <stdint.h>

#define LCD_I2C_ADDR 0x27
#define LCD_BUS 0

int lcd_init(struct i2c1722_link *link);
int lcd_print(struct i2c1722_link *link, const char *str);
int lcd_clear(struct i2c1722_link *link);
int lcd_set_cursor(struct i2c1722_link *link, uint8_t row, uint8_t col);

#endif