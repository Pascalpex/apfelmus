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

#define I2C1722_UDP_PORT 17220

// Example values
#define I2C1722_CONTROLLER_STREAM_ID 0xAABBCCDDEEFF0010ULL // controller talks on this
#define I2C1722_TARGET_STREAM_ID 0xAABBCCDDEEFF0011ULL     // target talks on this

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
