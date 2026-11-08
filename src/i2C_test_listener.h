#ifndef I2C_TEST_LISTENER_H
#define I2C_TEST_LISTENER_H

#include <zephyr/device.h>


/* Test creation and decoding of ACF-I2C messages */
void test_full_i2c_message(void);
void test_brief_i2c_message(void);

/* Test decoding of manually prepared raw messages */
void test_raw_brief_message(const struct device *i2c_dev);

/* Test the ACF-I2C listener */
void test_full_listener(const struct device *i2c_dev);
void test_brief_listener(const struct device *i2c_dev);


void test_full_fram_write(const struct device *i2c_dev);

void test_brief_fram_write(const struct device *i2c_dev);

void test_full_fram_read(const struct device *i2c_dev);

void test_brief_fram_read(const struct device *i2c_dev);

#endif /* I2C_TEST_LISTENER_H */