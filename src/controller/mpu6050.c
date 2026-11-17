/*
 * mpu6050.c - MPU-6050 accelerometer/gyroscope over the remote I2C API
 */
#include "mpu6050.h"

#define MPU6050_REG_PWR_MGMT_1 0x6B /* write 0 = wake up, internal oscillator */
#define MPU6050_REG_DATA       0x3B /* ACCEL_XOUT_H, 14 bytes follow */

static int16_t be16(const uint8_t *p)
{
    return (int16_t)((p[0] << 8) | p[1]);
}

int mpu6050_read_raw(struct i2c1722_link *link, struct mpu6050_raw *out)
{
    const uint8_t wake[2] = {MPU6050_REG_PWR_MGMT_1, 0x00};
    const uint8_t reg = MPU6050_REG_DATA;
    uint8_t d[14];

    if (i2c1722_write(link, MPU6050_BUS, MPU6050_ADDR, wake, sizeof(wake)) != 0) {
        return -1;
    }
    if (i2c1722_write_read(link, MPU6050_BUS, MPU6050_ADDR, &reg, 1, d, sizeof(d)) != 0) {
        return -1;
    }
    for (int i = 0; i < 3; i++) {
        out->accel[i] = be16(&d[2 * i]);
        out->gyro[i]  = be16(&d[8 + 2 * i]);
    }
    out->temp = be16(&d[6]);
    return 0;
}
