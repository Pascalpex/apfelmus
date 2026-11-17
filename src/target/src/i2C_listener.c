#include "i2C_listener.h"
#include "i2c1722.h"

#include <zephyr/drivers/i2c.h>
#include <stdbool.h>
#include <string.h>

#define WRITE_BUF_SIZE 256
#define READ_BUF_SIZE  64

/**
 * @brief Stores the state and data of an ongoing I2C transaction.
 *
 * Device independent: write bytes (CR3) are buffered and executed on the bus
 * at STOP (CR4) or at the first read (CR6/CR7), because Zephyr executes a
 * whole I2C transaction in one call.
 */
typedef struct {
    uint8_t address;
    bool read;              /* read phase: CR1 with R bit, or after CR5 */

    uint8_t write_data[WRITE_BUF_SIZE];
    size_t write_length;

    uint8_t read_data[READ_BUF_SIZE];
    size_t read_length;
    size_t read_pos;
} i2c_transaction_t;

/** Holds the state of the currently active ACF-I2C transaction. */
static i2c_transaction_t current_transaction;

/* Creates the response: copies transaction number, event and bus ID from the request. */
static void create_response(const Avtp_I2C_t *request, Avtp_I2C_t *response,
                            uint8_t response_code, uint8_t response_data)
{
    Avtp_I2C_CreateAcfMessage(response, response_code, false,
                              Avtp_I2C_GetTransactionNum(request), Avtp_I2C_GetEvt(request),
                              AVTP_I2C_EXC_NONE, response_data);
    Avtp_I2C_SetI2CBusId(response, Avtp_I2C_GetI2CBusId(request));
}

/*
 * Number of bytes to read for the read starting at requests[index]:
 * the CR6 messages that follow plus the final CR7.
 */
static size_t count_reads(const Avtp_I2C_t *requests, size_t num_requests, size_t index)
{
    size_t n = 0;
    for (size_t i = index; i < num_requests; i++) {
        uint8_t code = Avtp_I2C_GetI2CCode(&requests[i]);
        n++;
        if (code != AVTP_I2C_CODE_CR6) {
            break; /* CR7 = last byte */
        }
    }
    return n;
}

/**
 * @brief Executes requests[index] and creates its response, device independent.
 *
 * CR1 START + address, CR3 write byte, CR5 repeated START + read address,
 * CR6/CR7 read byte (CR7 = last byte + STOP), CR4 STOP.
 * The whole request list is passed so a read can look ahead for its length.
 * Assumes a correct request sequence and a working bus (no error handling).
 *
 * @return true if a response was created.
 */
static bool execute_request(const struct device *i2c_dev, const Avtp_I2C_t *requests,
                            size_t num_requests, size_t index, Avtp_I2C_t *response)
{
    const Avtp_I2C_t *request = &requests[index];
    i2c_transaction_t *t = &current_transaction;
    uint8_t data = Avtp_I2C_GetI2CData(request);
    uint8_t rsp_code = AVTP_I2C_CODE_TR2;
    uint8_t rsp_data = 0;

    switch (Avtp_I2C_GetI2CCode(request)) {
    case AVTP_I2C_CODE_CR1: /* START + address byte: new transaction */
        memset(t, 0, sizeof(*t));
        t->address = data >> 1;
        t->read = (data & 0x01U) != 0U;
        break;

    case AVTP_I2C_CODE_CR3: /* buffer one write byte */
        t->write_data[t->write_length++] = data;
        break;

    case AVTP_I2C_CODE_CR5: /* repeated START: read phase */
        t->read = true;
        break;

    case AVTP_I2C_CODE_CR6: /* read one byte */
    case AVTP_I2C_CODE_CR7: /* read last byte, NACK + STOP */
        /* First read of this phase: execute the transaction on the bus */
        if (t->read_pos >= t->read_length) {
            size_t n = count_reads(requests, num_requests, index);
            if (t->write_length > 0) {
                (void)i2c_write_read(i2c_dev, t->address, t->write_data, t->write_length,
                                     t->read_data, n);
            } else {
                (void)i2c_read(i2c_dev, t->read_data, n, t->address);
            }
            t->write_length = 0;
            t->read_length = n;
            t->read_pos = 0;
        }
        rsp_code = AVTP_I2C_CODE_TR3;
        rsp_data = t->read_data[t->read_pos++];
        break;

    case AVTP_I2C_CODE_CR4: /* STOP: execute the buffered write */
        if (!t->read && t->write_length > 0) {
            (void)i2c_write(i2c_dev, t->write_data, t->write_length, t->address);
        }
        rsp_code = AVTP_I2C_CODE_TR5;
        break;

    default:
        return false;
    }

    create_response(request, response, rsp_code, rsp_data);
    return true;
}

size_t i2c_listener_execute(const struct device *i2c_dev, const Avtp_I2C_t *requests,
                            Avtp_I2C_t *responses, size_t num_requests)
{
    size_t rsp_count = 0;

    for (size_t i = 0; i < num_requests; i++) {
        if (execute_request(i2c_dev, requests, num_requests, i, &responses[rsp_count])) {
            rsp_count++;
        }
    }
    return rsp_count;
}
