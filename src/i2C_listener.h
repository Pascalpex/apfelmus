#ifndef I2C_LISTENER_H
#define I2C_LISTENER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <string.h>
#include "fram.h"
#include "bh1750.h"

#include "../Open1722/include/avtp/acf/I2C.h"
#include "../Open1722/include/avtp/acf/I2CBrief.h"

typedef enum {
    I2C_FORMAT_FULL,
    I2C_FORMAT_BRIEF
} i2c_message_format_t;

int i2c_listener_process(
    const struct device *i2c_dev,
    i2c_message_format_t format,
    const void *request,
    size_t request_size,
    void *response,
    size_t response_size
);
    
#endif