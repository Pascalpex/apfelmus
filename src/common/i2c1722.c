/*
 * i2c1722.c - packing / unpacking of ACF_I2C messages into IEEE 1722 NTSCF frames
 */
#include <errno.h>
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

/* ---------------- Remote I2C API (controller side) ---------------- */

/* Appends one request message (TRR set, so the target answers every message). */
static void add_request(Avtp_I2C_t *msgs, size_t *n, uint8_t code, uint8_t txn, uint16_t bus_id,
                        uint8_t data)
{
    Avtp_I2C_CreateAcfMessage(&msgs[*n], code, true, txn, 0, AVTP_I2C_EXC_NONE, data);
    Avtp_I2C_SetI2CBusId(&msgs[*n], bus_id);
    (*n)++;
}

/* Write transaction: CR1 (addr+W), CR3 per data byte, CR4 (STOP). Returns -1 if too long. */
static int build_write(Avtp_I2C_t *msgs, uint8_t txn, uint16_t bus_id, uint8_t addr,
                       const uint8_t *data, size_t len)
{
    size_t n = 0;

    if (len + 2 > I2C1722_MAX_MSGS)
    {
        return -1;
    }
    add_request(msgs, &n, AVTP_I2C_CODE_CR1, txn, bus_id, (uint8_t)(addr << 1));
    for (size_t i = 0; i < len; i++)
    {
        add_request(msgs, &n, AVTP_I2C_CODE_CR3, txn, bus_id, data[i]);
    }
    add_request(msgs, &n, AVTP_I2C_CODE_CR4, txn, bus_id, 0);
    return (int)n;
}

/*
 * Read transaction. With wlen > 0: CR1 (addr+W), CR3 per write byte, CR5 (addr+R);
 * with wlen == 0: CR1 (addr+R). Then CR6 per read byte and CR7 for the last one.
 * Returns -1 if too long.
 */
static int build_write_read(Avtp_I2C_t *msgs, uint8_t txn, uint16_t bus_id, uint8_t addr,
                            const uint8_t *wdata, size_t wlen, size_t rlen)
{
    size_t n = 0;

    if (rlen == 0 || wlen + rlen + 2 > I2C1722_MAX_MSGS)
    {
        return -1;
    }
    if (wlen > 0)
    {
        add_request(msgs, &n, AVTP_I2C_CODE_CR1, txn, bus_id, (uint8_t)(addr << 1));
        for (size_t i = 0; i < wlen; i++)
        {
            add_request(msgs, &n, AVTP_I2C_CODE_CR3, txn, bus_id, wdata[i]);
        }
        add_request(msgs, &n, AVTP_I2C_CODE_CR5, txn, bus_id, (uint8_t)((addr << 1) | 1));
    }
    else
    {
        add_request(msgs, &n, AVTP_I2C_CODE_CR1, txn, bus_id, (uint8_t)((addr << 1) | 1));
    }
    for (size_t i = 0; i + 1 < rlen; i++)
    {
        add_request(msgs, &n, AVTP_I2C_CODE_CR6, txn, bus_id, 0);
    }
    add_request(msgs, &n, AVTP_I2C_CODE_CR7, txn, bus_id, 0);
    return (int)n;
}

/* Returns 0 if no response is TR1 and exactly rlen TR3 bytes were collected, -1 otherwise. */
static int parse_response(const Avtp_I2C_t *rsp, int n, uint8_t *rdata, size_t rlen)
{
    size_t got = 0;

    for (int i = 0; i < n; i++)
    {
        uint8_t code = Avtp_I2C_GetI2CCode(&rsp[i]);
        if (code == AVTP_I2C_CODE_TR1)
        {
            return -1;
        }
        if (code == AVTP_I2C_CODE_TR3 && got < rlen)
        {
            rdata[got++] = Avtp_I2C_GetI2CData(&rsp[i]);
        }
    }
    return got == rlen ? 0 : -1;
}

/*
 * Sends the requests in one frame, waits for the answer to this transaction
 * and evaluates it (TR1 -> -EIO, TR3 bytes -> rdata).
 * Answers to older transactions (e.g. arriving after a timeout) are dropped.
 * Returns 0 or a negative errno.
 */
static int request(struct i2c1722_link *link, const Avtp_I2C_t *req, int nreq,
                   uint8_t *rdata, size_t rlen)
{
    uint8_t pdu[I2C1722_PDU_SIZE(I2C1722_MAX_MSGS)];
    Avtp_I2C_t rsp[I2C1722_MAX_MSGS];

    if (nreq < 0)
    {
        return -EINVAL; /* transaction does not fit into one frame */
    }
    uint8_t txn = Avtp_I2C_GetTransactionNum(&req[0]);

    int len = i2c_to_avtp(pdu, sizeof(pdu), link->use_udp, AVTPDU_VERSION_0,
                          I2C1722_CONTROLLER_STREAM_ID, link->seq++, req, (uint8_t)nreq);
    if (len < 0)
    {
        return -EINVAL;
    }
    int ret = link->send(link->ctx, pdu, (size_t)len);
    if (ret < 0)
    {
        return ret;
    }

    for (;;)
    {
        int rx_len = link->recv(link->ctx, pdu, sizeof(pdu));
        if (rx_len < 0)
        {
            return rx_len;
        }

        uint32_t rsp_seq;
        int m = avtp_to_i2c(pdu, (size_t)rx_len, link->use_udp, I2C1722_TARGET_STREAM_ID, rsp,
                            I2C1722_MAX_MSGS, &rsp_seq);
        if (m > 0 && Avtp_I2C_GetTransactionNum(&rsp[0]) == txn)
        {
            return parse_response(rsp, m, rdata, rlen) == 0 ? 0 : -EIO;
        }
        /* invalid frame or answer to another transaction: keep waiting */
    }
}

int i2c1722_write(struct i2c1722_link *link, uint16_t bus_id, uint8_t addr,
                  const uint8_t *buf, size_t len)
{
    Avtp_I2C_t req[I2C1722_MAX_MSGS];
    int n = build_write(req, ++link->txn, bus_id, addr, buf, len);
    return request(link, req, n, NULL, 0);
}

int i2c1722_read(struct i2c1722_link *link, uint16_t bus_id, uint8_t addr,
                 uint8_t *buf, size_t len)
{
    return i2c1722_write_read(link, bus_id, addr, NULL, 0, buf, len);
}

int i2c1722_write_read(struct i2c1722_link *link, uint16_t bus_id, uint8_t addr,
                       const uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen)
{
    Avtp_I2C_t req[I2C1722_MAX_MSGS];
    int n = build_write_read(req, ++link->txn, bus_id, addr, wbuf, wlen, rlen);
    return request(link, req, n, rbuf, rlen);
}
