/*
 * bh1750.c - BH1750 ambient light sensor over the remote I2C API
 */
#include <unistd.h>

#include "bh1750.h"

#define BH1750_ONETIME_HR 0x20 /* one-time measurement, 1 lx resolution */
#define BH1750_WAIT_US    (200 * 1000) /* conversion time max. 180 ms */

int bh1750_read_raw(struct i2c1722_link *link, uint16_t *raw)
{
    const uint8_t cmd = BH1750_ONETIME_HR;
    uint8_t data[2];

    if (i2c1722_write(link, BH1750_BUS, BH1750_ADDR, &cmd, 1) != 0) {
        return -1;
    }
    usleep(BH1750_WAIT_US);

    if (i2c1722_read(link, BH1750_BUS, BH1750_ADDR, data, sizeof(data)) != 0) {
        return -1;
    }
    *raw = (uint16_t)((data[0] << 8) | data[1]);
    return 0;
}
