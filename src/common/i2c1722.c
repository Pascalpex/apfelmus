/*
 * i2c1722.c - packing / unpacking of ACF_I2C messages into IEEE 1722 NTSCF frames
 */
#include <string.h>

#include "i2c1722.h"

int i2c_to_avtp(uint8_t *pdu, size_t pdu_size, int use_udp, uint8_t ntscf_version,
                uint64_t stream_id, uint32_t seq_num, const Avtp_I2C_t *msgs, uint8_t num_msgs)
{
    size_t offset = use_udp ? AVTP_UDP_HEADER_LEN : 0;
    size_t cf_header_len = (ntscf_version == AVTPDU_VERSION_1) ? AVTP_NTSCF_HEADER_LEN_V1
                                                               : AVTP_NTSCF_HEADER_LEN_V0;
    size_t acf_len = (size_t)num_msgs * AVTP_I2C_HEADER_LEN;

    size_t max_acf_len =
        (1U << Avtp_NtscfFieldDescV0[AVTP_NTSCF_FIELD_NTSCF_DATA_LENGTH].bits) - 1;

    if (pdu == NULL || msgs == NULL || num_msgs == 0 || acf_len > max_acf_len ||
        pdu_size < offset + cf_header_len + acf_len)
    {
        return -1;
    }

    // 1. UDP encapsulation header
    if (use_udp)
    {
        Avtp_Udp_Init((Avtp_Udp_t *)pdu);
        Avtp_Udp_SetEncapsulationSeqNo((Avtp_Udp_t *)pdu, seq_num);
    }

    // 2. NTSCF header
    Avtp_Ntscf_t *ntscf = (Avtp_Ntscf_t *)(pdu + offset);
    Avtp_Ntscf_Init(ntscf, ntscf_version);
    Avtp_Ntscf_SetSequenceNum(ntscf, seq_num);
    Avtp_Ntscf_SetStreamId(ntscf, stream_id);

    // 3. ACF_I2C messages
    memcpy((uint8_t *)ntscf + cf_header_len, msgs, acf_len);

    // 4. NTSCF data length
    Avtp_Ntscf_SetNtscfDataLength(ntscf, (uint16_t)acf_len);

    return (int)(offset + cf_header_len + acf_len);
}

int avtp_to_i2c(const uint8_t *pdu, size_t pdu_length, int use_udp, uint64_t stream_id,
                Avtp_I2C_t *msgs, uint8_t max_msgs, uint32_t *seq_num)
{
    size_t offset = use_udp ? AVTP_UDP_HEADER_LEN : 0;

    if (pdu == NULL || msgs == NULL || seq_num == NULL || pdu_length < offset)
    {
        return -1;
    }

    const Avtp_Ntscf_t *ntscf = (const Avtp_Ntscf_t *)(pdu + offset);
    if (!Avtp_Ntscf_IsValid(ntscf, pdu_length - offset) ||
        Avtp_Ntscf_GetStreamId(ntscf) != stream_id)
    {
        return -1;
    }
    *seq_num = Avtp_Ntscf_GetSequenceNum(ntscf);

    const uint8_t *acf = (const uint8_t *)ntscf + Avtp_Ntscf_GetHeaderLen(ntscf);
    size_t remaining = Avtp_Ntscf_GetNtscfDataLength(ntscf);
    uint8_t n = 0;

    while (remaining > 0)
    {
        if (n >= max_msgs || !Avtp_I2C_IsValid((const Avtp_I2C_t *)acf, remaining))
        {
            return -1;
        }
        memcpy(&msgs[n++], acf, AVTP_I2C_HEADER_LEN);
        acf += AVTP_I2C_HEADER_LEN;
        remaining -= AVTP_I2C_HEADER_LEN;
    }
    return n;
}
