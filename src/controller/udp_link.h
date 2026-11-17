/*
 * udp_link.h - IEEE 1722 over UDP transport for the remote I2C API (i2c1722_link)
 */
#pragma once

#include "i2c1722.h"

/**
 * Opens a UDP socket to the target and sets up link to use it.
 *
 * @param link Link to initialize.
 * @param host Target IP address or hostname (e.g. apfelmus.local).
 * @return 0, or -1 on error (message printed).
 */
int udp_link_open(struct i2c1722_link *link, const char *host);

/** Closes the socket of link. */
void udp_link_close(struct i2c1722_link *link);
