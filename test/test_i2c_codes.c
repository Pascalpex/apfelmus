/*
 * Unit tests for the ACF_I2C code values in src/common/avtp_i2c_codes.h.
 * Written in the style of the Open1722 unit tests (unit/test-i2c.c).
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <string.h>

#include "avtp/acf/I2C.h"
#include "avtp/acf/I2CBrief.h"
#include "avtp_i2c_codes.h"

#define MAX_PDU_SIZE 1500

/* Table 27 and Table 28 */
static void i2c_code_values(void **state)
{
    assert_int_equal(AVTP_I2C_CODE_CR1, 0x0);
    assert_int_equal(AVTP_I2C_CODE_CR2, 0x1);
    assert_int_equal(AVTP_I2C_CODE_CR3, 0x2);
    assert_int_equal(AVTP_I2C_CODE_CR4, 0x3);
    assert_int_equal(AVTP_I2C_CODE_CR5, 0x4);
    assert_int_equal(AVTP_I2C_CODE_CR6, 0x5);
    assert_int_equal(AVTP_I2C_CODE_CR7, 0x6);
    assert_int_equal(AVTP_I2C_CODE_CR8, 0x7);
    assert_int_equal(AVTP_I2C_CODE_TR1, 0x8);
    assert_int_equal(AVTP_I2C_CODE_TR2, 0x9);
    assert_int_equal(AVTP_I2C_CODE_TR3, 0xA);
    assert_int_equal(AVTP_I2C_CODE_TR4, 0xB);
    assert_int_equal(AVTP_I2C_CODE_TR5, 0xC);
}

/* Table 29 */
static void i2c_exception_values(void **state)
{
    assert_int_equal(AVTP_I2C_EXC_NONE, 0x0);
    assert_int_equal(AVTP_I2C_EXC_USER_FIRST, 0x4);
    assert_int_equal(AVTP_I2C_EXC_USER_LAST, 0x7);
    assert_int_equal(AVTP_I2C_EXC_BUS_TIMEOUT, 0x8);
    assert_int_equal(AVTP_I2C_EXC_BUS_BUSY, 0x9);
    assert_int_equal(AVTP_I2C_EXC_CONTROLLER_CONFLICT, 0xA);
    assert_int_equal(AVTP_I2C_EXC_TRANSACTION_NUM, 0xB);
    assert_int_equal(AVTP_I2C_EXC_START_ERROR, 0xC);
    assert_int_equal(AVTP_I2C_EXC_SEQUENCE_ERROR, 0xD);
}

/* All codes have to fit into the 4-bit i2c_code field. */
static void i2c_code_round_trip(void **state)
{
    uint8_t pdu[MAX_PDU_SIZE];

    for (uint8_t code = AVTP_I2C_CODE_CR1; code <= AVTP_I2C_CODE_TR5; code++) {
        Avtp_I2C_CreateAcfMessage((Avtp_I2C_t *)pdu, code, false, 0, 0, AVTP_I2C_EXC_NONE, 0);
        assert_int_equal(Avtp_I2C_GetI2CCode((Avtp_I2C_t *)pdu), code);

        Avtp_I2CBrief_CreateAcfMessage((Avtp_I2CBrief_t *)pdu, code, false, 0, 0, AVTP_I2C_EXC_NONE,
                                       0);
        assert_int_equal(Avtp_I2CBrief_GetI2CCode((Avtp_I2CBrief_t *)pdu), code);
    }
}

/* Same for the 4-bit exception_code field. */
static void i2c_exception_round_trip(void **state)
{
    uint8_t pdu[MAX_PDU_SIZE];

    for (uint8_t exc = AVTP_I2C_EXC_NONE; exc <= AVTP_I2C_EXC_SEQUENCE_ERROR; exc++) {
        Avtp_I2C_CreateAcfMessage((Avtp_I2C_t *)pdu, AVTP_I2C_CODE_TR1, false, 0, 0, exc, 0);
        assert_int_equal(Avtp_I2C_GetExceptionCode((Avtp_I2C_t *)pdu), exc);

        Avtp_I2CBrief_CreateAcfMessage((Avtp_I2CBrief_t *)pdu, AVTP_I2C_CODE_TR1, false, 0, 0, exc,
                                       0);
        assert_int_equal(Avtp_I2CBrief_GetExceptionCode((Avtp_I2CBrief_t *)pdu), exc);
    }
}

/*
 * TR4 (read acknowledge with data) after a timeout. i2c_code is in the upper
 * nibble of the first byte of the last quadlet, exception_code in the lower
 * nibble of the third byte (Figure 89 and Figure 91).
 */
static void i2c_code_wire_format(void **state)
{
    uint8_t pdu[MAX_PDU_SIZE];

    memset(pdu, 0, sizeof(pdu));
    Avtp_I2C_CreateAcfMessage((Avtp_I2C_t *)pdu, AVTP_I2C_CODE_TR4, false, 0x12, 0,
                              AVTP_I2C_EXC_BUS_TIMEOUT, 0x5A);
    assert_int_equal(pdu[12], 0xB0);
    assert_int_equal(pdu[13], 0x12);
    assert_int_equal(pdu[14], 0x08);
    assert_int_equal(pdu[15], 0x5A);

    memset(pdu, 0, sizeof(pdu));
    Avtp_I2CBrief_CreateAcfMessage((Avtp_I2CBrief_t *)pdu, AVTP_I2C_CODE_TR4, false, 0x12, 0,
                                   AVTP_I2C_EXC_BUS_TIMEOUT, 0x5A);
    assert_int_equal(pdu[4], 0xB0);
    assert_int_equal(pdu[5], 0x12);
    assert_int_equal(pdu[6], 0x08);
    assert_int_equal(pdu[7], 0x5A);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* values from the tables in the standard */
        cmocka_unit_test(i2c_code_values),
        cmocka_unit_test(i2c_exception_values),
        /* values used in actual messages */
        cmocka_unit_test(i2c_code_round_trip),
        cmocka_unit_test(i2c_exception_round_trip),
        cmocka_unit_test(i2c_code_wire_format),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
