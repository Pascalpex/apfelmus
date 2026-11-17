#ifndef I2C_LISTENER_H
#define I2C_LISTENER_H

#include <stddef.h>
#include <zephyr/device.h>

#include <avtp/acf/I2C.h>

/**
 * Executes a list of ACF_I2C requests on the I2C bus (device independent).
 * The bus ID is copied into the responses; everything runs on i2c_dev.
 * Assumes correct requests and a working bus (no error handling yet).
 * Responses are packed densely into responses (one per request).
 * The transaction state is kept between calls, so a transaction may span lists.
 *
 * @return Number of response messages.
 */
size_t i2c_listener_execute(
    const struct device *i2c_dev,
    const Avtp_I2C_t *requests,
    Avtp_I2C_t *responses,
    size_t num_requests
);

#endif
