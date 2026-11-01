#ifndef BH1750_H
#define BH1750_H

#include <zephyr/device.h>
#include <stdint.h>

#define BH1750_I2C_ADDR       0x23
#define BH1750_ONETIME_HR     0x20

/*
 * Perform one light measurement.
 *
 * i2c_dev: I2C controller
 * lux_x100: output value in hundredths of lux
 *
 * Example:
 * 12545 means 125.45 lux.
 *
 * Returns 0 on success, negative error code on failure.
 */
int bh1750_read_lux(
    const struct device *i2c_dev,
    uint32_t *lux_x100
);

#endif