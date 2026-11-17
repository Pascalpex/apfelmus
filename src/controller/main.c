/* Controller (laptop): reads the temperature of the I2C sensor on the target over IEEE 1722. */
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "i2c1722.h"
#include "avtp_i2c_codes.h"

#define MAX_I2C_MSGS 16

#define FRAM_ADDR  0x50
#define BH1750_ADDR 0x23

/* Appends one ACF_I2C message and returns the new count. */
static int add_msg(Avtp_I2C_t *msgs, int n,    uint8_t code,
    bool trr, uint8_t txn, uint8_t evt, uint8_t data)
{
    Avtp_I2C_CreateAcfMessage(&msgs[n], code, trr, txn, evt, /*TODO*/ 0, data);
    Avtp_I2C_SetI2CBusId(&msgs[n], 0);
    return n + 1;
}

/*
 * Sends one frame of request messages and receives the response.
 * Returns the number of response messages, or -1 on error.
 */
static int transfer(int fd, const struct addrinfo *target, uint32_t seq,
                    const Avtp_I2C_t *req, int nreq, Avtp_I2C_t *rsp)
{
    // Pack into a 1722 frame and send
    uint8_t pdu[I2C1722_PDU_SIZE(MAX_I2C_MSGS)];
    int len = i2c_to_avtp(pdu, sizeof(pdu), 1, AVTPDU_VERSION_0, I2C1722_CONTROLLER_STREAM_ID,
                          seq, req, (uint8_t)nreq);
    if (send(fd, pdu, (size_t)len, 0) < 0) {
        perror("send");
        return -1;
    }
    printf("Sent %d bytes (%d ACF_I2C messages)\n", len, nreq);

    // Receive and unpack the response
    uint8_t rx[I2C1722_PDU_SIZE(MAX_I2C_MSGS)];
    ssize_t n = recv(fd, rx, sizeof(rx), 0);
    if (n < 0)
    {
        printf("No response\n");
        return -1;
    }

    uint32_t rsp_seq;
    int m = avtp_to_i2c(rx, (size_t)n, 1, I2C1722_TARGET_STREAM_ID, rsp, MAX_I2C_MSGS, &rsp_seq);
    if (m < 0)
    {
        printf("Invalid response (%zd bytes)\n", n);
        return -1;
    }

    printf("Response seq=%u: %d ACF_I2C message(s)\n", rsp_seq, m);
    for (int i = 0; i < m; i++)
    {
        printf("  [%d] bus=%u code=0x%x trr=%d txn=%u evt=0x%x exc=0x%x data=0x%02x\n", i,
               Avtp_I2C_GetI2CBusId(&rsp[i]), Avtp_I2C_GetI2CCode(&rsp[i]),
               Avtp_I2C_IsTrr(&rsp[i]), Avtp_I2C_GetTransactionNum(&rsp[i]),
               Avtp_I2C_GetEvt(&rsp[i]), Avtp_I2C_GetExceptionCode(&rsp[i]),
               Avtp_I2C_GetI2CData(&rsp[i]));
    }
    return m;
}

static void fram_write(
    int fd,
    const struct addrinfo *target,
    uint32_t seq
)
{
    Avtp_I2C_t req[MAX_I2C_MSGS];
    Avtp_I2C_t rsp[MAX_I2C_MSGS];

    int n = 0;
    uint8_t txn = 1;

    n = add_msg(req, n, AVTP_I2C_CODE_CR1, true, txn, 0,
                (FRAM_ADDR << 1));

    n = add_msg(req, n, AVTP_I2C_CODE_CR3, true, txn, 0, 0x00);
    n = add_msg(req, n, AVTP_I2C_CODE_CR3, true, txn, 0, 0x10);
    n = add_msg(req, n, AVTP_I2C_CODE_CR3, true, txn, 0, 0x42);

    n = add_msg(req, n, AVTP_I2C_CODE_CR4, true, txn, 0, 0);

    transfer(fd, target, seq, req, n, rsp);
}

static void fram_read(
    int fd,
    const struct addrinfo *target,
    uint32_t seq
)
{
    Avtp_I2C_t req[MAX_I2C_MSGS];
    Avtp_I2C_t rsp[MAX_I2C_MSGS];

    int n = 0;
    uint8_t txn = 2;

    n = add_msg(req, n, AVTP_I2C_CODE_CR1, true, txn, 0,
                (FRAM_ADDR << 1));

    n = add_msg(req, n, AVTP_I2C_CODE_CR3, true, txn, 0, 0x00);
    n = add_msg(req, n, AVTP_I2C_CODE_CR3, true, txn, 0, 0x10);

    n = add_msg(req, n, AVTP_I2C_CODE_CR5, true, txn, 0,
                (FRAM_ADDR << 1) | 1);

    n = add_msg(req, n, AVTP_I2C_CODE_CR6, true, txn, 0, 0);
    n = add_msg(req, n, AVTP_I2C_CODE_CR7, true, txn, 0, 0);

    int count = transfer(fd, target, seq, req, n, rsp);

    if (count > 0) {
        printf("FRAM read completed. Check the returned TR3 data bytes.\n");
    }
}

static void bh1750_read(
    int fd,
    const struct addrinfo *target,
    uint32_t seq
)
{
    Avtp_I2C_t req[MAX_I2C_MSGS];
    Avtp_I2C_t rsp[MAX_I2C_MSGS];

    int n = 0;
    uint8_t txn = 3;

    /* Start measurement */
    n = add_msg(req, n, AVTP_I2C_CODE_CR1, true, txn, 0,
                (BH1750_ADDR << 1));

    n = add_msg(req, n, AVTP_I2C_CODE_CR3, true, txn, 0, 0x20);

    n = add_msg(req, n, AVTP_I2C_CODE_CR4, true, txn, 0, 0);

    /* Read measurement */
    n = add_msg(req, n, AVTP_I2C_CODE_CR1, true, txn, 0,
                (BH1750_ADDR << 1) | 1);

    n = add_msg(req, n, AVTP_I2C_CODE_CR6, true, txn, 0, 0);
    n = add_msg(req, n, AVTP_I2C_CODE_CR7, true, txn, 0, 0);

    int count = transfer(fd, target, seq, req, n, rsp);

    if (count > 0) {
        printf("BH1750 response received. Check the returned TR3 data bytes.\n");
    }
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <target-ip-or-hostname> [fram-write|fram-read|bh1750]\n",
                argv[0]);
        return 1;
    }

    // Demo to run, default: fram-write
    const char *demo = (argc > 2) ? argv[2] : "fram-write";
    if (strcmp(demo, "fram-write") != 0 && strcmp(demo, "fram-read") != 0 &&
        strcmp(demo, "bh1750") != 0)
    {
        fprintf(stderr, "Unknown demo '%s' (fram-write, fram-read or bh1750)\n", demo);
        return 1;
    }

    // Target address: IP or hostname (apfelmus.local)
    char port[6];
    snprintf(port, sizeof(port), "%d", I2C1722_UDP_PORT);
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
    struct addrinfo *target;
    if (getaddrinfo(argv[1], port, &hints, &target) != 0)
    {
        fprintf(stderr, "Cannot resolve %s\n", argv[1]);
        return 1;
    }

    // UDP socket, wait at most 1 s for the response
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    if (connect(fd, target->ai_addr, target->ai_addrlen) < 0) {
        perror("connect");
        close(fd);
        freeaddrinfo(target);
        return 1;
    }

    struct timeval timeout = {.tv_sec = 1};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (strcmp(demo, "fram-write") == 0)
    {
        fram_write(fd, target, 0);
    }
    else if (strcmp(demo, "fram-read") == 0)
    {
        fram_read(fd, target, 0);
    }
    else
    {
        bh1750_read(fd, target, 0);
    }

    freeaddrinfo(target);
    close(fd);
    return 0;
}
