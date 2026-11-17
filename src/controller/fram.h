/*
 * fram.h - MB85RC256V FRAM (32 KB) over the remote I2C API
 */
#pragma once

#include "i2c1722.h"

#define FRAM_BUS  0    /* I2C bus on the target */
#define FRAM_ADDR 0x50 /* I2C address */
#define FRAM_SIZE 32768

/**
 * Writes len bytes to memory address mem (split into several transactions).
 * @return 0, or -1 on error.
 */
int fram_write(struct i2c1722_link *link, uint16_t mem, const uint8_t *data, size_t len);

/**
 * Reads len bytes from memory address mem (split into several transactions).
 * @return 0, or -1 on error.
 */
int fram_read(struct i2c1722_link *link, uint16_t mem, uint8_t *data, size_t len);
