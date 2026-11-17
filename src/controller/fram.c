/*
 * fram.c - MB85RC256V FRAM (32 KB) over the remote I2C API
 *
 * Write: [mem high] [mem low] [data...]
 * Read:  [mem high] [mem low], repeated START, read data
 */
#include <string.h>

#include "fram.h"

#define FRAM_CHUNK 32 /* data bytes per I2C transaction */

int fram_write(struct i2c1722_link *link, uint16_t mem, const uint8_t *data, size_t len)
{
    uint8_t buf[2 + FRAM_CHUNK];

    if ((size_t)mem + len > FRAM_SIZE) {
        return -1;
    }
    while (len > 0) {
        size_t c = len < FRAM_CHUNK ? len : FRAM_CHUNK;
        buf[0] = (uint8_t)(mem >> 8);   /* memory address, big endian */
        buf[1] = (uint8_t)(mem & 0xFF);
        memcpy(&buf[2], data, c);
        if (i2c1722_write(link, FRAM_BUS, FRAM_ADDR, buf, c + 2) != 0) {
            return -1;
        }
        mem += (uint16_t)c;
        data += c;
        len -= c;
    }
    return 0;
}

int fram_read(struct i2c1722_link *link, uint16_t mem, uint8_t *data, size_t len)
{
    if ((size_t)mem + len > FRAM_SIZE) {
        return -1;
    }
    while (len > 0) {
        size_t c = len < FRAM_CHUNK ? len : FRAM_CHUNK;
        const uint8_t addr[2] = {(uint8_t)(mem >> 8), (uint8_t)(mem & 0xFF)};
        if (i2c1722_write_read(link, FRAM_BUS, FRAM_ADDR, addr, sizeof(addr), data, c) != 0) {
            return -1;
        }
        mem += (uint16_t)c;
        data += c;
        len -= c;
    }
    return 0;
}
