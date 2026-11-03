#ifndef I2C_LISTENER_H
#define I2C_LISTENER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../Open1722/include/avtp/acf/I2C.h"
#include "../Open1722/include/avtp/acf/I2CBrief.h"

typedef enum {
    I2C_FORMAT_FULL,
    I2C_FORMAT_BRIEF
} i2c_message_format_t;

/*
 * Process one decoded ACF-I2C request and create its response.
 *
 * Returns 0 on success, negative value on error.
 */
int i2c_listener_process(
    i2c_message_format_t format,
    const void *request,
     size_t request_size,
    void *response,
    size_t response_size
);

#endif