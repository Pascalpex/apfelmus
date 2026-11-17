/* Target Controller (board): receives ACF_I2C messages over IEEE 1722 / UDP. */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>
#include "i2C_listener.h"

#include "i2c1722.h"

LOG_MODULE_REGISTER(i2c1722_target, LOG_LEVEL_INF);


/* I2C bus the requests are executed on (ACF_I2C bus ID 0) */
static const struct device *i2c_bus = DEVICE_DT_GET(DT_ALIAS(i2c1722_bus));

/* static: keeps the buffers off the main stack */
static uint8_t rx_buf[I2C1722_PDU_SIZE(I2C1722_MAX_MSGS)];
static Avtp_I2C_t msgs[I2C1722_MAX_MSGS];
static uint8_t tx_buf[I2C1722_PDU_SIZE(I2C1722_MAX_MSGS)];
static Avtp_I2C_t rsp_msgs[I2C1722_MAX_MSGS];
static uint32_t tx_seq; /* sequence number of the target's own stream */

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
        ssize_t len = recvfrom(fd, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&src, &src_len); //if a message is received it is stored in rx_buf and the length of the message is returned in len
        if (len < 0)
        {
            LOG_ERR("recvfrom() failed: %d", errno);
            continue;
        }

        uint32_t seq;
        // extracts the ACF-I2C messages from the received UDP packet
        int n = avtp_to_i2c(rx_buf, (size_t)len, 1, I2C1722_CONTROLLER_STREAM_ID, msgs,
                            I2C1722_MAX_MSGS, &seq);
                            
        if (n < 0) {
            LOG_ERR("Invalid number of I2C messages: %d", n);
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
        LOG_INF("Frame seq=%u from %s: %d ACF_I2C message(s)", seq, ip, n);

        /* Run the requests on the I2C bus, one response message per request message */
        size_t rsp_count = i2c_listener_execute(i2c_bus, msgs, rsp_msgs, (size_t)n);

        // pack the i2C response messages into an ACF-I2C frame and send it back to the requester
        /* Encode and send only if responses exist */
        if (rsp_count > 0) {
            int rsp_len = i2c_to_avtp(tx_buf, sizeof(tx_buf), 1, AVTPDU_VERSION_0,
                                  I2C1722_TARGET_STREAM_ID, tx_seq++, rsp_msgs, (uint8_t)rsp_count);
            if (rsp_len < 0 ||
                sendto(fd, tx_buf, (size_t)rsp_len, 0, (struct sockaddr *)&src, src_len) < 0)
            {
                LOG_ERR("Sending response failed: %d", errno);
                continue;
            }
            LOG_INF("Response seq=%u sent to %s: %d ACF_I2C message(s)", tx_seq - 1, ip, (int)rsp_count);
        }
    }
    return 0;
}
