/*
 * mpu6050.h - MPU-6050 accelerometer/gyroscope over the remote I2C API
 */
#pragma once

#include "i2c1722.h"

#define MPU6050_BUS  0    /* I2C bus on the target */
#define MPU6050_ADDR 0x68 /* I2C address (AD0 pin low) */

/* Raw -> physical units (default ranges: +-2 g, +-250 deg/s) */
#define MPU6050_ACCEL_TO_G(raw)   ((raw) / 16384.0)
#define MPU6050_GYRO_TO_DPS(raw)  ((raw) / 131.0)
#define MPU6050_TEMP_TO_C(raw)    ((raw) / 340.0 + 36.53)

struct mpu6050_raw {
    int16_t accel[3]; /* x, y, z */
    int16_t temp;
    int16_t gyro[3];  /* x, y, z */
};

/**
 * Wakes the sensor and reads accel, temperature and gyro in one burst.
 * @return 0, or -1 on error.
 */
int mpu6050_read_raw(struct i2c1722_link *link, struct mpu6050_raw *out);
