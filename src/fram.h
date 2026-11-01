#ifndef FRAM_H
#define FRAM_H

#include <zephyr/device.h>
#include <stdint.h>
#include <stddef.h>

/* MB85RC256V parameters */
#define FRAM_I2C_ADDR  0x50
#define FRAM_CAPACITY  32768

/*
 * Write data to a specified FRAM memory address.
 *
 * i2c_dev:       I2C controller
 * fram_address:  Starting address inside FRAM
 * data:          Data to write
 * length:        Number of bytes to write
 *
 * Returns 0 on success, negative error code on failure.
 */



int write_to_fram(
    const struct device *i2c_dev,
    uint16_t fram_address,
    const uint8_t *data,
    size_t length
);

/*
 * Read data from a specified FRAM memory address.
 *
 * i2c_dev:       I2C controller
 * fram_address:  Starting address inside FRAM
 * data:          Buffer for the received data
 * length:        Number of bytes to read
 *
 * Returns 0 on success, negative error code on failure.
 */
int read_fram(
    const struct device *i2c_dev,
    uint16_t fram_address,
    uint8_t *data,
    size_t length
);

#endif