/*
 * i2c1722.h - shared definitions for I2C over IEEE 1722 (ACF_I2C, IEEE 1722-2025 §9.4.16)
 *
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#include "avtp/Udp.h"
#include "avtp/acf/Ntscf.h"
#include "avtp/acf/AcfCommon.h"
#include "avtp/acf/I2C.h"
#include "avtp/CommonHeader.h"

#include "avtp_i2c_codes.h" /* AVTP_I2C_CODE_* and AVTP_I2C_EXC_* values */

#define I2C1722_UDP_PORT 17220

// Example values
#define I2C1722_CONTROLLER_STREAM_ID 0xAABBCCDDEEFF0010ULL // controller talks on this
#define I2C1722_TARGET_STREAM_ID 0xAABBCCDDEEFF0011ULL     // target talks on this

/* Max. ACF_I2C messages per frame (64 * 16 B = 1 KB, below the Ethernet MTU) */
#define I2C1722_MAX_MSGS 64

/* Buffer size for a frame with num_msgs ACF_I2C messages */
#define I2C1722_PDU_SIZE(num_msgs) \
    (AVTP_UDP_HEADER_LEN + AVTP_NTSCF_HEADER_LEN_V1 + (num_msgs) * AVTP_I2C_HEADER_LEN)

/**
 * Packs finished ACF_I2C messages into one IEEE 1722 NTSCF frame.
 *
 * @param pdu           Output buffer.
 * @param pdu_size      Size of the output buffer, see I2C1722_PDU_SIZE().
 * @param use_udp       Non-zero: prepend the 1722 UDP encapsulation header.
 * @param ntscf_version AVTPDU_VERSION_0 or AVTPDU_VERSION_1.
 * @param stream_id     Stream ID of the sending side (talker stream).
 * @param seq_num       Sequence number (NTSCF, and UDP encapsulation if used).
 * @param msgs          ACF_I2C messages to send.
 * @param num_msgs      Number of messages in msgs (>= 1).
 * @return Number of bytes to send, or -1 on error.
 */
int i2c_to_avtp(uint8_t *pdu, size_t pdu_size, int use_udp, uint8_t ntscf_version,
                uint64_t stream_id, uint32_t seq_num, const Avtp_I2C_t *msgs, uint8_t num_msgs);

/**
 * Checks a received IEEE 1722 frame and copies its ACF_I2C messages out.
 * Frames of other streams, invalid frames and frames with non-I2C ACF messages are rejected.
 *
 * @param pdu        Received data.
 * @param pdu_length Number of received bytes.
 * @param use_udp    Non-zero: the frame starts with the 1722 UDP encapsulation header.
 * @param stream_id  Stream ID to accept (listener stream).
 * @param msgs       Output array for the ACF_I2C messages.
 * @param max_msgs   Number of entries in msgs.
 * @param seq_num    Output: NTSCF sequence number of the frame.
 * @return Number of ACF_I2C messages copied to msgs, or -1 on error.
 */
int avtp_to_i2c(const uint8_t *pdu, size_t pdu_length, int use_udp, uint64_t stream_id,
                Avtp_I2C_t *msgs, uint8_t max_msgs, uint32_t *seq_num);

/*
 * Remote I2C API (controller side): works like the local Zephyr I2C API
 * (i2c_write / i2c_read / i2c_write_read), but the transaction is executed
 * on the target over IEEE 1722. Each call blocks until the answer arrives.
 *
 * The transport is provided by the application through two callbacks, so this
 * library contains no socket code (UDP, raw Ethernet, ... all work).
 * One transaction must fit into one frame (see I2C1722_MAX_MSGS).
 */

struct i2c1722_link {
    /* Sends one frame. Returns 0 or a negative errno. */
    int (*send)(void *ctx, const uint8_t *pdu, size_t len);
    /* Receives one frame. Returns its length, -ETIMEDOUT if nothing arrived, or a negative errno. */
    int (*recv)(void *ctx, uint8_t *pdu, size_t max_len);
    void *ctx;       /* passed to send/recv, e.g. the socket fd */
    int use_udp;     /* non-zero: frames carry the 1722 UDP encapsulation header */

    /* Managed by the library, initialize to 0 */
    uint32_t seq;    /* sequence number of the next frame */
    uint8_t txn;     /* transaction number of the last request */
};

/**
 * Writes len bytes to the device addr on bus bus_id: START, addr+W, data, STOP.
 * @return 0, -EIO on NACK/bus error, -ETIMEDOUT if no answer, -EINVAL if it does not fit into one frame.
 */
int i2c1722_write(struct i2c1722_link *link, uint16_t bus_id, uint8_t addr,
                  const uint8_t *buf, size_t len);

/**
 * Reads len bytes from the device addr on bus bus_id: START, addr+R, data, STOP.
 * @return see i2c1722_write().
 */
int i2c1722_read(struct i2c1722_link *link, uint16_t bus_id, uint8_t addr,
                 uint8_t *buf, size_t len);

/**
 * Writes wlen bytes, then reads rlen bytes with a repeated START in between.
 * @return see i2c1722_write().
 */
int i2c1722_write_read(struct i2c1722_link *link, uint16_t bus_id, uint8_t addr,
                       const uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen);
