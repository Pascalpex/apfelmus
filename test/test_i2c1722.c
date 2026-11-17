/*
 * Unit tests for packing / unpacking ACF_I2C messages into IEEE 1722 NTSCF frames
 * (src/common/i2c1722.c). Written in the style of the Open1722 unit tests.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#if defined(__cplusplus)
extern "C" {
#include <cmocka.h>
}
#else
#include <cmocka.h>
#endif
#include <string.h>

#include "i2c1722.h"
#include "avtp_i2c_codes.h"

#define MAX_MSGS 4

static void create_msg(Avtp_I2C_t *msg, uint8_t code, uint8_t txn, uint8_t data)
{
    Avtp_I2C_CreateAcfMessage(msg, code, false, txn, 0, AVTP_I2C_EXC_NONE, data);
    Avtp_I2C_SetI2CBusId(msg, 0);
}

/* One request packed with UDP encapsulation, byte by byte (Figure 89, NTSCF header). */
static void i2c1722_wire_format(void **state)
{
    uint8_t pdu[I2C1722_PDU_SIZE(1)];
    Avtp_I2C_t msg;
    const uint8_t expected[] = {
        0x00, 0x00, 0x00, 0x00, /* UDP encapsulation: encapsulation_seq_no = 0 */
        0x82, 0x80, 0x10, 0x00, /* NTSCF: subtype, sv = 1, ntscf_data_length = 16, seq = 0 */
        0xAA, 0xBB, 0xCC, 0xDD, /* stream_id */
        0xEE, 0xFF, 0x00, 0x10,
        0x1E, 0x04, 0x00, 0x00, /* ACF_I2C: acf_msg_type, acf_msg_length = 4, mtv = 0 */
        0x00, 0x00, 0x00, 0x00, /* message_timestamp */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x05, 0x00, 0x90, /* CR1, transaction_num = 5, address 0x48 + write */
    };

    memset(pdu, 0, sizeof(pdu));
    create_msg(&msg, AVTP_I2C_CODE_CR1, 0x05, 0x48 << 1);

    assert_int_equal(i2c_to_avtp(pdu, sizeof(pdu), 1, AVTPDU_VERSION_0,
                                 I2C1722_CONTROLLER_STREAM_ID, 0, &msg, 1),
                     sizeof(expected));
    assert_memory_equal(pdu, expected, sizeof(expected));
}

/* Several messages in one frame, with and without UDP encapsulation. */
static void i2c1722_round_trip(void **state)
{
    uint8_t pdu[I2C1722_PDU_SIZE(MAX_MSGS)];
    Avtp_I2C_t req[3];
    Avtp_I2C_t rsp[MAX_MSGS];
    uint32_t seq = 0;

    create_msg(&req[0], AVTP_I2C_CODE_CR1, 1, 0x50 << 1);
    create_msg(&req[1], AVTP_I2C_CODE_CR3, 2, 0x00);
    create_msg(&req[2], AVTP_I2C_CODE_CR3, 3, 0x10);

    for (int use_udp = 0; use_udp <= 1; use_udp++) {
        int len = i2c_to_avtp(pdu, sizeof(pdu), use_udp, AVTPDU_VERSION_0,
                              I2C1722_CONTROLLER_STREAM_ID, 42, req, 3);
        assert_true(len > 0);

        assert_int_equal(avtp_to_i2c(pdu, (size_t)len, use_udp, I2C1722_CONTROLLER_STREAM_ID, rsp,
                                     MAX_MSGS, &seq),
                         3);
        assert_int_equal(seq, 42);
        assert_memory_equal(rsp, req, sizeof(req));
    }
}

/* NTSCF version 1 has a longer header (28 instead of 12 bytes). */
static void i2c1722_round_trip_v1(void **state)
{
    uint8_t pdu[I2C1722_PDU_SIZE(MAX_MSGS)];
    Avtp_I2C_t req;
    Avtp_I2C_t rsp[MAX_MSGS];
    uint32_t seq = 0;

    create_msg(&req, AVTP_I2C_CODE_CR7, 9, 0);

    int len =
        i2c_to_avtp(pdu, sizeof(pdu), 1, AVTPDU_VERSION_1, I2C1722_TARGET_STREAM_ID, 7, &req, 1);
    assert_int_equal(len, AVTP_UDP_HEADER_LEN + AVTP_NTSCF_HEADER_LEN_V1 + AVTP_I2C_HEADER_LEN);

    assert_int_equal(
        avtp_to_i2c(pdu, (size_t)len, 1, I2C1722_TARGET_STREAM_ID, rsp, MAX_MSGS, &seq), 1);
    assert_int_equal(seq, 7);
    assert_memory_equal(&rsp[0], &req, sizeof(req));
}

static void i2c1722_pack_invalid(void **state)
{
    uint8_t pdu[I2C1722_PDU_SIZE(MAX_MSGS)];
    Avtp_I2C_t req[2];

    create_msg(&req[0], AVTP_I2C_CODE_CR1, 1, 0x50 << 1);
    create_msg(&req[1], AVTP_I2C_CODE_CR4, 2, 0);

    /* missing buffers, no messages */
    assert_int_equal(i2c_to_avtp(NULL, sizeof(pdu), 1, AVTPDU_VERSION_0,
                                 I2C1722_CONTROLLER_STREAM_ID, 0, req, 2),
                     -1);
    assert_int_equal(i2c_to_avtp(pdu, sizeof(pdu), 1, AVTPDU_VERSION_0,
                                 I2C1722_CONTROLLER_STREAM_ID, 0, NULL, 2),
                     -1);
    assert_int_equal(
        i2c_to_avtp(pdu, sizeof(pdu), 1, AVTPDU_VERSION_0, I2C1722_CONTROLLER_STREAM_ID, 0, req, 0),
        -1);

    /* buffer one byte too small for two messages (I2C1722_PDU_SIZE() is sized for NTSCF v1) */
    const size_t needed = AVTP_UDP_HEADER_LEN + AVTP_NTSCF_HEADER_LEN_V0 + 2 * AVTP_I2C_HEADER_LEN;
    assert_int_equal(
        i2c_to_avtp(pdu, needed, 1, AVTPDU_VERSION_0, I2C1722_CONTROLLER_STREAM_ID, 0, req, 2),
        (int)needed);
    assert_int_equal(
        i2c_to_avtp(pdu, needed - 1, 1, AVTPDU_VERSION_0, I2C1722_CONTROLLER_STREAM_ID, 0, req, 2),
        -1);
}

static void i2c1722_unpack_invalid(void **state)
{
    uint8_t pdu[I2C1722_PDU_SIZE(MAX_MSGS)];
    Avtp_I2C_t req[2];
    Avtp_I2C_t rsp[MAX_MSGS];
    uint32_t seq;

    create_msg(&req[0], AVTP_I2C_CODE_CR1, 1, 0x50 << 1);
    create_msg(&req[1], AVTP_I2C_CODE_CR4, 2, 0);
    int len =
        i2c_to_avtp(pdu, sizeof(pdu), 1, AVTPDU_VERSION_0, I2C1722_CONTROLLER_STREAM_ID, 0, req, 2);
    assert_true(len > 0);

    /* frame of the other stream */
    assert_int_equal(
        avtp_to_i2c(pdu, (size_t)len, 1, I2C1722_TARGET_STREAM_ID, rsp, MAX_MSGS, &seq), -1);

    /* frame shorter than announced in ntscf_data_length */
    assert_int_equal(
        avtp_to_i2c(pdu, (size_t)len - 1, 1, I2C1722_CONTROLLER_STREAM_ID, rsp, MAX_MSGS, &seq),
        -1);

    /* more messages than fit into the output array */
    assert_int_equal(avtp_to_i2c(pdu, (size_t)len, 1, I2C1722_CONTROLLER_STREAM_ID, rsp, 1, &seq),
                     -1);

    /* missing output pointers */
    assert_int_equal(
        avtp_to_i2c(pdu, (size_t)len, 1, I2C1722_CONTROLLER_STREAM_ID, NULL, MAX_MSGS, &seq), -1);
    assert_int_equal(
        avtp_to_i2c(pdu, (size_t)len, 1, I2C1722_CONTROLLER_STREAM_ID, rsp, MAX_MSGS, NULL), -1);
}

/* Frames with an ACF message other than ACF_I2C are rejected. */
static void i2c1722_unpack_other_acf_type(void **state)
{
    uint8_t pdu[I2C1722_PDU_SIZE(MAX_MSGS)];
    Avtp_I2C_t req;
    Avtp_I2C_t rsp[MAX_MSGS];
    uint32_t seq;

    create_msg(&req, AVTP_I2C_CODE_CR1, 1, 0x50 << 1);
    Avtp_AcfCommon_SetAcfMsgType((Avtp_AcfCommon_t *)&req, AVTP_ACF_TYPE_CAN);

    int len = i2c_to_avtp(pdu, sizeof(pdu), 1, AVTPDU_VERSION_0, I2C1722_CONTROLLER_STREAM_ID, 0,
                          &req, 1);
    assert_true(len > 0);
    assert_int_equal(
        avtp_to_i2c(pdu, (size_t)len, 1, I2C1722_CONTROLLER_STREAM_ID, rsp, MAX_MSGS, &seq), -1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* valid frames */
        cmocka_unit_test(i2c1722_wire_format),
        cmocka_unit_test(i2c1722_round_trip),
        cmocka_unit_test(i2c1722_round_trip_v1),
        /* invalid input and frames */
        cmocka_unit_test(i2c1722_pack_invalid),
        cmocka_unit_test(i2c1722_unpack_invalid),
        cmocka_unit_test(i2c1722_unpack_other_acf_type),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
