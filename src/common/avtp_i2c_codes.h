/*
 * Values of the i2c_code and exception_code fields of the ACF_I2C and
 * ACF_I2C_BRIEF messages (IEEE 1722-2025, clauses 9.4.16 and 9.4.17).
 *
 * Open1722 only provides the field accessors for these messages
 * (avtp/acf/I2C.h, avtp/acf/I2CBrief.h). The naming follows the value enums
 * in Open1722 (e.g. Avtp_CrfType_t in avtp/Crf.h), so the enums can later be
 * moved to avtp/acf/I2C.h without changing the code that uses them.
 */

#pragma once

/**
 * ACF_I2C 'i2c_code' field values (IEEE 1722-2025, Table 27 and Table 28).
 * CR1-CR8 are requests sent by the Target Agent, TR1-TR5 are responses sent by
 * the Target Controller. Values 0xD-0xF are reserved. ACF_I2C_BRIEF uses the
 * same values.
 */
typedef enum {
    AVTP_I2C_CODE_CR1 = 0x0, /* START, i2c_data = address and R/W bit */
    AVTP_I2C_CODE_CR2 = 0x1, /* address continuation for 10-bit addresses */
    AVTP_I2C_CODE_CR3 = 0x2, /* write continue, i2c_data = data byte */
    AVTP_I2C_CODE_CR4 = 0x3, /* write end (STOP) */
    AVTP_I2C_CODE_CR5 = 0x4, /* write end and restart, i2c_data = address and R/W bit */
    AVTP_I2C_CODE_CR6 = 0x5, /* read continue (ACK) */
    AVTP_I2C_CODE_CR7 = 0x6, /* read end (NACK and STOP) */
    AVTP_I2C_CODE_CR8 = 0x7, /* read ACK and restart, i2c_data = address and R/W bit */
    AVTP_I2C_CODE_TR1 = 0x8, /* NACK */
    AVTP_I2C_CODE_TR2 = 0x9, /* ACK */
    AVTP_I2C_CODE_TR3 = 0xA, /* read data, i2c_data = data byte */
    AVTP_I2C_CODE_TR4 = 0xB, /* read acknowledge with data, i2c_data = data byte */
    AVTP_I2C_CODE_TR5 = 0xC, /* STOP confirmation, only if trr was set in CR4 or CR7 */
} Avtp_I2CCode_t;

/**
 * ACF_I2C 'exception_code' field values (IEEE 1722-2025, Table 29). Values
 * 0x1-0x3 and 0xE-0xF are reserved, 0x4-0x7 are user defined.
 *
 * Like Avtp_AcfMsgType_t, this enum only documents the standardized values.
 * Because of the user-defined range, the accessors keep the raw uint8_t type.
 */
typedef enum {
    AVTP_I2C_EXC_NONE = 0x0,
    AVTP_I2C_EXC_USER_FIRST = 0x4,
    AVTP_I2C_EXC_USER_LAST = 0x7,
    AVTP_I2C_EXC_BUS_TIMEOUT = 0x8,         /* no ack or data from the target in time */
    AVTP_I2C_EXC_BUS_BUSY = 0x9,            /* bus did not become idle, no START possible */
    AVTP_I2C_EXC_CONTROLLER_CONFLICT = 0xA, /* i2c_bus_id differs from the initial request */
    AVTP_I2C_EXC_TRANSACTION_NUM = 0xB,     /* unexpected transaction_num */
    AVTP_I2C_EXC_START_ERROR = 0xC,         /* request other than START while the bus is idle */
    AVTP_I2C_EXC_SEQUENCE_ERROR = 0xD,      /* request does not fit the previous request */
} Avtp_I2CException_t;
