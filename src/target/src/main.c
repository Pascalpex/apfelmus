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

#define MAX_I2C_MSGS 16

/* I2C bus the requests are executed on*/
static const struct device *i2c_bus = DEVICE_DT_GET(DT_ALIAS(i2c1722_bus));

/* static: keeps the buffers off the main stack */
static uint8_t rx_buf[I2C1722_PDU_SIZE(MAX_I2C_MSGS)];
static Avtp_I2C_t msgs[MAX_I2C_MSGS];
static uint8_t tx_buf[I2C1722_PDU_SIZE(MAX_I2C_MSGS)];
static Avtp_I2C_t rsp_msgs[MAX_I2C_MSGS];
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



static size_t execute_i2c(
    const struct device *i2c_bus,
    const Avtp_I2C_t *req,
    Avtp_I2C_t *rsp,
    int n)
{
     size_t rsp_count = 0;
    // process each request message and generate a response message
    for (int i = 0; i < n; i++) {

        // store the response in the next free slot, rsp[rsp_count]
        int ret = i2c_listener_process(
            i2c_bus,
            I2C_FORMAT_FULL,
            &req[i],
            sizeof(req[i]),
            &rsp[rsp_count],
            sizeof(rsp[rsp_count])
        );

        if (ret == 1) {
            rsp_count++;
        } else if (ret < 0) {
            LOG_ERR("I2C request %d failed: %d", i, ret);
        }
    }
    return rsp_count; //the listener returns 1 whenever it has filled a response structure
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
                            MAX_I2C_MSGS, &seq);
                            
        if (n < 0 || n > MAX_I2C_MSGS) {
            LOG_ERR("Invalid number of I2C messages: %d", n);
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

        /* Run the requests on the I2C bus, one response message per request message */
        size_t rsp_count = execute_i2c(i2c_bus, msgs, rsp_msgs, n);

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
