#pragma once
/*
 * i2c1722_proto.h - meaning of the ACF_I2C fields shared by controller and target
 *
 * PROVISIONAL: these values are a placeholder encoding to get a working example.
 * TODO: replace them with the code points defined in IEEE 1722-2025 section 9.4.16.
 *
 */

/* evt field */
#define I2C1722_EVT_NONE 0x0
#define I2C1722_EVT_START 0x1

/* R/W bit of the address byte */
#define I2C1722_WRITE 0x0
#define I2C1722_READ 0x1
#define I2C1722_ADDR_BYTE(addr, rw) ((uint8_t)(((addr) << 1) | (rw)))

/* exception_code field */
#define I2C1722_EXC_OK 0x0
#define I2C1722_EXC_NACK 0x1
