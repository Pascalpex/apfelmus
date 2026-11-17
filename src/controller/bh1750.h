/*
 * bh1750.h - BH1750 ambient light sensor over the remote I2C API
 */
#pragma once

#include "i2c1722.h"

#define BH1750_BUS  0    /* I2C bus on the target */
#define BH1750_ADDR 0x23 /* I2C address (ADDR pin low) */

/* Raw value -> lux (high resolution mode) */
#define BH1750_RAW_TO_LUX(raw) ((raw) / 1.2)

/**
 * Starts a one-time high resolution measurement, waits for it (~200 ms)
 * and reads the raw 16-bit result.
 * @return 0, or -1 on error.
 */
int bh1750_read_raw(struct i2c1722_link *link, uint16_t *raw);
