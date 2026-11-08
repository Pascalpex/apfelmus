/* Controller (laptop): sends one ACF_I2C message to the target and prints the response. */
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "i2c1722.h"

#define MAX_I2C_MSGS 16

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <target-ip-or-hostname>\n", argv[0]);
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
    struct timeval timeout = {.tv_sec = 1};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // One ACF_I2C message with test values
    Avtp_I2C_t req;
    Avtp_I2C_CreateAcfMessage(&req, 0, false, 1, 0, 0, 0x42);
    Avtp_I2C_SetI2CBusId(&req, 0);

    // Pack into a 1722 frame and send
    uint8_t pdu[I2C1722_PDU_SIZE(1)];
    int len = i2c_to_avtp(pdu, sizeof(pdu), 1, AVTPDU_VERSION_0, I2C1722_CONTROLLER_STREAM_ID, 0, &req, 1);
    sendto(fd, pdu, (size_t)len, 0, target->ai_addr, target->ai_addrlen);
    printf("Sent %d bytes to %s\n", len, argv[1]);

    // Receive and unpack the response
    uint8_t rx[I2C1722_PDU_SIZE(MAX_I2C_MSGS)];
    ssize_t n = recv(fd, rx, sizeof(rx), 0);
    if (n < 0)
    {
        printf("No response\n");
        return 1;
    }

    Avtp_I2C_t rsp[MAX_I2C_MSGS];
    uint32_t seq;
    int m = avtp_to_i2c(rx, (size_t)n, 1, I2C1722_TARGET_STREAM_ID, rsp, MAX_I2C_MSGS, &seq);
    if (m < 0)
    {
        printf("Invalid response (%zd bytes)\n", n);
        return 1;
    }

    printf("Response seq=%u: %d ACF_I2C message(s)\n", seq, m);
    for (int i = 0; i < m; i++)
    {
        printf("  [%d] bus=%u code=0x%x trr=%d txn=%u evt=0x%x exc=0x%x data=0x%02x\n", i,
               Avtp_I2C_GetI2CBusId(&rsp[i]), Avtp_I2C_GetI2CCode(&rsp[i]),
               Avtp_I2C_IsTrr(&rsp[i]), Avtp_I2C_GetTransactionNum(&rsp[i]),
               Avtp_I2C_GetEvt(&rsp[i]), Avtp_I2C_GetExceptionCode(&rsp[i]),
               Avtp_I2C_GetI2CData(&rsp[i]));
    }

    freeaddrinfo(target);
    close(fd);
    return 0;
}
