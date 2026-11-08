/* Target Controller (board): receives ACF_I2C messages over IEEE 1722 / UDP. */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>

#include "i2c1722.h"

LOG_MODULE_REGISTER(i2c1722_target, LOG_LEVEL_INF);

#define MAX_I2C_MSGS 16

/* static: keeps the buffers off the main stack */
static uint8_t rx_buf[I2C1722_PDU_SIZE(MAX_I2C_MSGS)];
static Avtp_I2C_t msgs[MAX_I2C_MSGS];

/* from create_listener_socket_udp() in Open1722 examples/acf-can/zephyr */
static int create_listener_socket_udp(uint32_t udp_port)
{

    int fd, res;
    struct sockaddr_in sk_addr;

    // create a UDP socket
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
    {
        perror("Failed to open socket");
        return -1;
    }

    // Initialize the socket
    memset((char *)&sk_addr, 0, sizeof(sk_addr));
    sk_addr.sin_family = AF_INET;
    sk_addr.sin_port = htons(udp_port);
    sk_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    res = bind(fd, (struct sockaddr *)&sk_addr, sizeof(sk_addr));
    if (res < 0)
    {
        perror("Couldn't bind() to port");
        close(fd);
        return -1;
    }

    return fd;
}

int main(void)
{
    LOG_INF("i2c1722 target");

    int fd = create_listener_socket_udp(I2C1722_UDP_PORT);
    if (fd < 0)
    {
        return 0;
    }
    LOG_INF("Listening for ACF_I2C on UDP port %d", I2C1722_UDP_PORT);

    for (;;)
    {
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);
        ssize_t len = recvfrom(fd, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&src, &src_len);
        if (len < 0)
        {
            LOG_ERR("recvfrom() failed: %d", errno);
            continue;
        }

        uint32_t seq;
        int n = avtp_to_i2c(rx_buf, (size_t)len, 1, I2C1722_CONTROLLER_STREAM_ID, msgs,
                            MAX_I2C_MSGS, &seq);
        if (n < 0)
        {
            LOG_WRN("Dropped %d-byte frame (not a valid ACF_I2C frame of our stream)", (int)len);
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
        LOG_INF("Frame seq=%u from %s: %d ACF_I2C message(s)", seq, ip, n);

        for (int i = 0; i < n; i++)
        {
            const Avtp_I2C_t *m = &msgs[i];
            LOG_INF("  [%d] bus=%u code=0x%x trr=%d txn=%u evt=0x%x exc=0x%x data=0x%02x", i,
                    Avtp_I2C_GetI2CBusId(m), Avtp_I2C_GetI2CCode(m), Avtp_I2C_IsTrr(m),
                    Avtp_I2C_GetTransactionNum(m), Avtp_I2C_GetEvt(m),
                    Avtp_I2C_GetExceptionCode(m), Avtp_I2C_GetI2CData(m));
        }
    }
    return 0;
}
