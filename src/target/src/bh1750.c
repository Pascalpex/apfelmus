#include "bh1750.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <errno.h>

/*
 * Perform one high-resolution light measurement.
 */
int bh1750_read_lux(
    const struct device *i2c_dev,
    uint32_t *lux_x100)
{
    if (i2c_dev == NULL || lux_x100 == NULL) {
        return -EINVAL;
    }

    uint8_t command = BH1750_ONETIME_HR;
    uint8_t data[2];

    // 1. Start the measurement.
    int ret = i2c_write(
        i2c_dev,
        &command,
        sizeof(command),
        BH1750_I2C_ADDR
    );

    if (ret != 0) {
        return ret;
    }

    // 2. Wait for the measurement to finish.
    k_msleep(180);

    // 3. Read the two measurement bytes.
    ret = i2c_read(
        i2c_dev,
        data,
        sizeof(data),
        BH1750_I2C_ADDR
    );

    if (ret != 0) {
        return ret;
    }

    // 4. Combine the two bytes into one 16-bit value.
    uint16_t raw = ((uint16_t)data[0] << 8) | data[1];

    // 5. Convert raw measurement to hundredths of lux.
    // BH1750 conversion: lux = raw / 1.2
    *lux_x100 = ((uint32_t)raw * 250U + 1U) / 3U;

    return 0;
}

int bh1750_read_raw(
    const struct device *i2c_dev,
    uint16_t *raw_value)
{
    if (i2c_dev == NULL || raw_value == NULL) {
        return -EINVAL;
    }

    uint8_t command = BH1750_ONETIME_HR;
    uint8_t data[2];

    int ret = i2c_write(
        i2c_dev,
        &command,
        sizeof(command),
        BH1750_I2C_ADDR
    );

    if (ret != 0) {
        return ret;
    }

    k_msleep(180);

    ret = i2c_read(
        i2c_dev,
        data,
        sizeof(data),
        BH1750_I2C_ADDR
    );

    if (ret != 0) {
        return ret;
    }

    *raw_value =
        ((uint16_t)data[0] << 8) | data[1];

    return 0;
}