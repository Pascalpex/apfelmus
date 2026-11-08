/*
 * author: biroeser
 * date: 02.10.2026
 * task: Send and Receive data via I2C to/from FRAM.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include "../Open1722/include/avtp/acf/I2C.h"
#include "../Open1722/include/avtp/acf/I2CBrief.h"
#include "bh1750.h"
#include "fram.h"
#include "i2C_test_listener.h"
#include "i2C_listener.h"

#define I2C_NODE DT_NODELABEL(i2c1)
// FRAM device parameters
#define FRAM_ADDR 0x50 //7-Bit I2C address of the FRAM device
#define FRAM_TEST_ADDR 0x0000 // Address in FRAM to write and read data
#define TEST_VALUE 0xA5
//BH1750 device parameters
#define BH1750_ADDR       0x23 // connect the ADDR pin to GND for 0x23 or VCC for 0x5C
#define BH1750_ONETIME_HR 0x20 // after one measurement, the device goes to low-power sleep state (it does not measure continuously), high-resolution: The sensor uses a more precise measurement mode, with a typical resolution of 1 lux.

int main(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    int ret;
     uint32_t lux_x100;

     /* 1. Check that the I2C controller is ready. */
    if (!device_is_ready(i2c_dev)) {
        printk("FAIL: I2C controller is not ready.\n");
        return 0;
    }
    printk("I2C controller is ready.\n");

     /* 2. Read the BH1750 light measurement. */
    ret = bh1750_read_lux(i2c_dev, &lux_x100);

    if (ret != 0) {
        printk("ERROR: BH1750 measurement failed: %d\n", ret);
        return 0;
    }

     printk("Measured light: %u.%02u lux\n",
        (unsigned int)(lux_x100 / 100),
        (unsigned int)(lux_x100 % 100));


     /* 3. Convert the measurement into four bytes. */
        uint8_t fram_data[4] = {
        (uint8_t)(lux_x100 >> 24),
        (uint8_t)(lux_x100 >> 16),
        (uint8_t)(lux_x100 >> 8),
        (uint8_t)lux_x100
    };


    /* 4. Store the measurement in FRAM. */
    ret = write_to_fram(
        i2c_dev,
        FRAM_TEST_ADDR,
        fram_data,
        sizeof(fram_data)
    );

    if (ret != 0) {
        printk("ERROR: FRAM write failed: %d\n", ret);
        return 0;
    }

    printk("Measurement successfully stored in FRAM.\n");


    /* 5. Read the measurement back from FRAM. */
    uint8_t read_data[4];

    ret = read_fram(
        i2c_dev,
        FRAM_TEST_ADDR,
        read_data,
        sizeof(read_data)
    );

    if (ret != 0) {
        printk("ERROR: FRAM read failed: %d\n", ret);
        return 0;
    }

    /* 6. Reconstruct the 32-bit value from the four bytes */
    lux_x100 =
        ((uint32_t)read_data[0] << 24) |
        ((uint32_t)read_data[1] << 16) |
        ((uint32_t)read_data[2] << 8)  |
        (uint32_t)read_data[3];

    /* Convert lux * 100 back into lux */
    uint32_t lux_whole = lux_x100 / 100;
    uint32_t lux_decimal = lux_x100 % 100;
    printk("Light measurement from FRAM: %u.%02u lux\n",
        (unsigned int)lux_whole,
        (unsigned int)lux_decimal);


     ///7. create an ACF-I2C message and check if it is valid
    printk("\nStarting ACF-I2C tests...\n");

    /* Test Full and Brief message creation and decoding. */
    test_full_i2c_message();
    test_brief_i2c_message();

    /* Test the listener with generated and raw messages. */
    // test_raw_brief_message(i2c_dev);
    // test_full_listener(i2c_dev);
    // test_brief_listener(i2c_dev);

    test_full_fram_write(i2c_dev);
    test_full_fram_read(i2c_dev);
    test_full_fram_multibyte_read(i2c_dev);

    test_brief_fram_write(i2c_dev);
    test_brief_fram_read(i2c_dev);

    printk("\nACF-I2C tests completed.\n");

    return 0;
}
