#include "fram.h"

#include <zephyr/drivers/i2c.h>
#include <errno.h>
#include <string.h>

/*
 * Write data to the FRAM.
 *
 * The FRAM expects:
 * [2-byte memory address] [data bytes]
 *
 * We write at most 32 data bytes per I2C operation
 * to keep the temporary buffer small.
 */
int write_to_fram(
    const struct device *i2c_dev,
    uint16_t fram_address,
    const uint8_t *data,
    size_t length)
{
    if (i2c_dev == NULL || data == NULL || length == 0) {
        return -EINVAL;
    }

    if ((uint32_t)fram_address + length > FRAM_CAPACITY) {
        return -EINVAL;
    }

    uint8_t write_buffer[34];  // 2 address bytes + 32 data bytes

    size_t offset = 0;

    while (offset < length) {

        size_t chunk_size = length - offset;

        if (chunk_size > 32) {
            chunk_size = 32;
        }

        uint16_t current_address = fram_address + offset;

        // First two bytes: FRAM memory address.
        write_buffer[0] = (uint8_t)(current_address >> 8);
        write_buffer[1] = (uint8_t)(current_address & 0xFF);

        // Remaining bytes: data to store.
        memcpy(&write_buffer[2], &data[offset], chunk_size);

        int ret = i2c_write(
            i2c_dev,
            write_buffer,
            chunk_size + 2,
            FRAM_I2C_ADDR
        );

        if (ret != 0) {
            printk("FAIL: FRAM write failed: %d\n", ret);
            return ret;
        }

        offset += chunk_size;
    }

     printk("Write completed.\n");
    return 0;
}


/*
 * Read data from the FRAM.
 *
 * First, send the 16-bit memory address.
 * Then, read the requested number of bytes.
 */
int read_fram(
    const struct device *i2c_dev,
    uint16_t fram_address,
    uint8_t *data,
    size_t length)
{
    if (i2c_dev == NULL || data == NULL || length == 0) {
        return -EINVAL;
    }

    if ((uint32_t)fram_address + length > FRAM_CAPACITY) {
        return -EINVAL;
    }

    uint8_t address_buffer[2] = {
        (uint8_t)(fram_address >> 8),
        (uint8_t)(fram_address & 0xFF)
    };

    int ret = i2c_write_read(
        i2c_dev,
        FRAM_I2C_ADDR,
        address_buffer,
        sizeof(address_buffer),
        data,
        length
    );

    if (ret != 0) {
         printk("FAIL: FRAM read failed: %d\n", ret);
        return ret;
    }


    return ret;

}