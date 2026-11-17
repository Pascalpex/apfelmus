/*
 * udp_link.c - IEEE 1722 over UDP transport for the remote I2C API (i2c1722_link)
 */
#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "udp_link.h"

/* Max. wait for an answer; a normal answer takes a few ms */
#define UDP_LINK_TIMEOUT_US (200 * 1000)

/* ctx holds the socket fd */
#define CTX_FD(ctx) ((int)(intptr_t)(ctx))

static int udp_send(void *ctx, const uint8_t *pdu, size_t len)
{
    return send(CTX_FD(ctx), pdu, len, 0) < 0 ? -errno : 0;
}

static int udp_recv(void *ctx, uint8_t *pdu, size_t max_len)
{
    ssize_t n = recv(CTX_FD(ctx), pdu, max_len, 0);
    if (n < 0) {
        return (errno == EAGAIN || errno == EWOULDBLOCK) ? -ETIMEDOUT : -errno;
    }
    return (int)n;
}

int udp_link_open(struct i2c1722_link *link, const char *host)
{
    // Target address: IP or hostname
    char port[6];
    snprintf(port, sizeof(port), "%d", I2C1722_UDP_PORT);
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
    struct addrinfo *target;
    if (getaddrinfo(host, port, &hints, &target) != 0) {
        fprintf(stderr, "Cannot resolve %s\n", host);
        return -1;
    }

    // UDP socket connected to the target
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        freeaddrinfo(target);
        return -1;
    }
    if (connect(fd, target->ai_addr, target->ai_addrlen) < 0) {
        perror("connect");
        close(fd);
        freeaddrinfo(target);
        return -1;
    }
    freeaddrinfo(target);

    struct timeval timeout = {.tv_usec = UDP_LINK_TIMEOUT_US};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    memset(link, 0, sizeof(*link));
    link->send = udp_send;
    link->recv = udp_recv;
    link->ctx = (void *)(intptr_t)fd;
    link->use_udp = 1;
    return 0;
}

void udp_link_close(struct i2c1722_link *link)
{
    close(CTX_FD(link->ctx));
}
