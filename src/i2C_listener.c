#include "i2c_listener.h"
#include "fram.h"

#include <zephyr/drivers/i2c.h>
#include <errno.h>
#include <string.h>

/* ACF-I2C request codes */
#define I2C_CODE_CR1  0
#define I2C_CODE_CR3  2
#define I2C_CODE_CR4  3
#define I2C_CODE_CR5  4
#define I2C_CODE_CR7  6

/* ACF-I2C response codes */
#define I2C_CODE_TR1  8
#define I2C_CODE_TR2  9
#define I2C_CODE_TR3  10
#define I2C_CODE_TR5  12

#define I2C_EXCEPTION_NONE      0
#define I2C_EXCEPTION_SEQUENCE  13

#define FRAM_ADDR 0x50

/*
 * One FRAM write transaction:
 *   2 bytes for the FRAM memory address
 *   Up to 32 bytes of actual FRAM data
 */
typedef struct {
    bool active;
    uint8_t address;
    bool read;
    uint8_t write_data[34];
    size_t write_length;
    uint8_t transaction;
    uint16_t bus_id;
} i2c_transaction_t;

static i2c_transaction_t current_transaction;


/* Create a Full-format response and copy request identifiers. */
static void create_response(
    const Avtp_I2C_t *request,
    Avtp_I2C_t *response,
    uint8_t response_code,
    uint8_t exception_code,
    uint8_t response_data)
{
    Avtp_I2C_CreateAcfMessage(
        response,
        response_code,
        false,
        Avtp_I2C_GetTransactionNum(request),
        Avtp_I2C_GetEvt(request),
        exception_code,
        response_data
    );

    Avtp_I2C_SetI2CBusId(
        response,
        Avtp_I2C_GetI2CBusId(request)
    );
}


/*
 * Process one Full-format ACF-I2C request.
 *
 * CR1 starts a transaction.
 * CR3 adds one data byte.
 * CR4 executes the buffered FRAM write.
 */
static int process_full(
    const struct device *i2c_dev,
    const Avtp_I2C_t *request,
    size_t request_size,
    Avtp_I2C_t *response,
    size_t response_size)
{
    if (i2c_dev == NULL ||
        !device_is_ready(i2c_dev) ||
        response_size < sizeof(*response) ||
        !Avtp_I2C_IsValid(request, request_size)) {
        return -EINVAL;
    }

    uint8_t code = Avtp_I2C_GetI2CCode(request);
    uint8_t transaction = Avtp_I2C_GetTransactionNum(request);
    uint16_t bus_id = Avtp_I2C_GetI2CBusId(request);

    /* -------------------------------------------------
     * CR1: START and address the peripheral.
     * ------------------------------------------------- */
    if (code == I2C_CODE_CR1) {

        uint8_t address_data = Avtp_I2C_GetI2CData(request);
        uint8_t address = address_data >> 1;
        bool read = (address_data & 0x01U) != 0U;

        /* This first implementation supports FRAM writes only. */
        if (address != FRAM_ADDR || read) {
            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_NONE,
                0
            );
            return 0;
        }

        /* Start a new transaction. */
        memset(&current_transaction, 0, sizeof(current_transaction));

        current_transaction.active = true;
        current_transaction.address = address;
        current_transaction.read = false;
        current_transaction.transaction = transaction;
        current_transaction.bus_id = bus_id;

        create_response(
            request, response,
            I2C_CODE_TR2,
            I2C_EXCEPTION_NONE,
            0
        );

        return 0;
    }

    /* All following commands require an active transaction. */
    if (!current_transaction.active ||
        current_transaction.transaction != transaction ||
        current_transaction.bus_id != bus_id) {

        create_response(
            request, response,
            I2C_CODE_TR1,
            I2C_EXCEPTION_SEQUENCE,
            0
        );

        return 0;
    }

    /* -------------------------------------------------
     * CR3: Collect one write-data byte.
     * ------------------------------------------------- */
    if (code == I2C_CODE_CR3) {

        if (current_transaction.write_length >=
            sizeof(current_transaction.write_data)) {

            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        current_transaction.write_data[
            current_transaction.write_length
        ] = Avtp_I2C_GetI2CData(request);

        current_transaction.write_length++;

        create_response(
            request, response,
            I2C_CODE_TR2,
            I2C_EXCEPTION_NONE,
            0
        );

        return 0;
    }

    /* -------------------------------------------------
    * CR5: End write phase and restart with read address.
    * ------------------------------------------------- */
    if (code == I2C_CODE_CR5) {

        uint8_t address_data = Avtp_I2C_GetI2CData(request);
        uint8_t address = address_data >> 1;
        bool read = (address_data & 0x01U) != 0U;

        /* We expect FRAM address 0x50 with the read bit set. */
        if (address != FRAM_ADDR ||
            !read ||
            current_transaction.write_length < 2) {

            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        current_transaction.read = true;

        create_response(
            request, response,
            I2C_CODE_TR2,
            I2C_EXCEPTION_NONE,
            0
        );

        return 0;
    }

    /* -------------------------------------------------
     * CR4: STOP and execute the FRAM write.
     * ------------------------------------------------- */
    if (code == I2C_CODE_CR4) {

        if (current_transaction.write_length < 3) {
            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        uint16_t fram_address =
            ((uint16_t)current_transaction.write_data[0] << 8) |
            current_transaction.write_data[1];

        size_t data_length =
            current_transaction.write_length - 2;

        int ret = write_to_fram(
            i2c_dev,
            fram_address,
            &current_transaction.write_data[2],
            data_length
        );

        current_transaction.active = false;

        if (ret != 0) {
            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_NONE,
                0
            );
            return ret;
        }

        /*
         * TR5 is returned only when the requester asks
         * for STOP confirmation using TRR.
         */
        if (Avtp_I2C_IsTrr(request)) {
            create_response(
                request, response,
                I2C_CODE_TR5,
                I2C_EXCEPTION_NONE,
                0
            );
        }

        return 0;
    }

    /* -------------------------------------------------
    * CR7: Read final byte, NACK and STOP.
    * ------------------------------------------------- */
    if (code == I2C_CODE_CR7) {

        if (!current_transaction.read ||
            current_transaction.write_length < 2) {

            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        uint8_t memory_address[2] = {
            current_transaction.write_data[0],
            current_transaction.write_data[1]
        };

        uint8_t read_data = 0;

        int ret = i2c_write_read(
            i2c_dev,
            FRAM_ADDR,
            memory_address,
            sizeof(memory_address),
            &read_data,
            sizeof(read_data)
        );

        current_transaction.active = false;

        if (ret != 0) {
            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_NONE,
                0
            );

            return ret;
        }

        /* TR3 returns the byte read from the FRAM. */
        create_response(
            request, response,
            I2C_CODE_TR3,
            I2C_EXCEPTION_NONE,
            read_data
        );

        return 0;
    }

    /* Other commands are not implemented yet. */
    return -ENOTSUP;
}


/*
 * Process one Brief-format ACF-I2C request.
 *
 * CR1 starts a transaction.
 * CR3 adds one data byte.
 * CR4 executes the buffered FRAM write.
 */
static int process_brief(
    const struct device *i2c_dev,
    const Avtp_I2CBrief_t *request,
    size_t request_size,
    Avtp_I2CBrief_t *response,
    size_t response_size)
{
    if (i2c_dev == NULL ||
        !device_is_ready(i2c_dev) ||
        response_size < sizeof(*response) ||
        !Avtp_I2CBrief_IsValid(request, request_size)) {
        return -EINVAL;
    }

    uint8_t code = Avtp_I2CBrief_GetI2CCode(request);
    uint8_t transaction =
        Avtp_I2CBrief_GetTransactionNum(request);
    uint16_t bus_id =
        Avtp_I2CBrief_GetI2CBusId(request);

    /*
     * CR1: START and address the FRAM.
     */
    if (code == I2C_CODE_CR1) {

        uint8_t address_data =
            Avtp_I2CBrief_GetI2CData(request);

        uint8_t address = address_data >> 1;
        bool read = (address_data & 0x01U) != 0U;

        /* This implementation supports FRAM writes only. */
        if (address != FRAM_ADDR || read) {
            create_brief_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_NONE,
                0
            );
            return 0;
        }

        /* Start a new transaction. */
        memset(&current_transaction, 0,
               sizeof(current_transaction));

        current_transaction.active = true;
        current_transaction.address = address;
        current_transaction.read = false;
        current_transaction.transaction = transaction;
        current_transaction.bus_id = bus_id;

        create_brief_response(
            request, response,
            I2C_CODE_TR2,
            I2C_EXCEPTION_NONE,
            0
        );

        return 0;
    }

    /*
     * All following commands require an active transaction
     * with matching transaction number and bus ID.
     */
    if (!current_transaction.active ||
        current_transaction.transaction != transaction ||
        current_transaction.bus_id != bus_id) {

        create_brief_response(
            request, response,
            I2C_CODE_TR1,
            I2C_EXCEPTION_SEQUENCE,
            0
        );

        return 0;
    }

    /*
     * CR3: Collect one write-data byte.
     */
    if (code == I2C_CODE_CR3) {

        if (current_transaction.write_length >=
            sizeof(current_transaction.write_data)) {

            current_transaction.active = false;

            create_brief_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        current_transaction.write_data[
            current_transaction.write_length
        ] = Avtp_I2CBrief_GetI2CData(request);

        current_transaction.write_length++;

        create_brief_response(
            request, response,
            I2C_CODE_TR2,
            I2C_EXCEPTION_NONE,
            0
        );

        return 0;
    }

    /* -------------------------------------------------
    * CR5: End write phase and restart with read address.
    * ------------------------------------------------- */
    if (code == I2C_CODE_CR5) {

        uint8_t address_data =
            Avtp_I2CBrief_GetI2CData(request);

        uint8_t address = address_data >> 1;
        bool read = (address_data & 0x01U) != 0U;

        /* Expect FRAM address 0x50 with the read bit set. */
        if (address != FRAM_ADDR ||
            !read ||
            current_transaction.write_length < 2) {

            current_transaction.active = false;

            create_brief_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        current_transaction.read = true;

        create_brief_response(
            request, response,
            I2C_CODE_TR2,
            I2C_EXCEPTION_NONE,
            0
        );

        return 0;
    }

    /*
     * CR4: STOP and execute the physical FRAM write.
     */
    if (code == I2C_CODE_CR4) {

        if (current_transaction.write_length < 3) {
            current_transaction.active = false;

            create_brief_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        uint16_t fram_address =
            ((uint16_t)current_transaction.write_data[0] << 8) |
            current_transaction.write_data[1];

        size_t data_length =
            current_transaction.write_length - 2;

        int ret = write_to_fram(
            i2c_dev,
            fram_address,
            &current_transaction.write_data[2],
            data_length
        );

        current_transaction.active = false;

        if (ret != 0) {
            create_brief_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_NONE,
                0
            );
            return ret;
        }

        /*
         * Send TR5 only when TRR requests STOP confirmation.
         */
        if (Avtp_I2CBrief_IsTrr(request)) {
            create_brief_response(
                request, response,
                I2C_CODE_TR5,
                I2C_EXCEPTION_NONE,
                0
            );
        }

        return 0;
    }

    /* -------------------------------------------------
    * CR7: Read final byte, NACK and STOP.
    * ------------------------------------------------- */
    if (code == I2C_CODE_CR7) {

        if (!current_transaction.read ||
            current_transaction.write_length < 2) {

            current_transaction.active = false;

            create_brief_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        uint8_t memory_address[2] = {
            current_transaction.write_data[0],
            current_transaction.write_data[1]
        };

        uint8_t read_data = 0;

        int ret = i2c_write_read(
            i2c_dev,
            FRAM_ADDR,
            memory_address,
            sizeof(memory_address),
            &read_data,
            sizeof(read_data)
        );

        current_transaction.active = false;

        if (ret != 0) {

            create_brief_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_NONE,
                0
            );

            return ret;
        }

        /* TR3 returns the actual byte read from the FRAM. */
        create_brief_response(
            request, response,
            I2C_CODE_TR3,
            I2C_EXCEPTION_NONE,
            read_data
        );

        return 0;
    }
    return -ENOTSUP;
}


static void create_brief_response(
    const Avtp_I2CBrief_t *request,
    Avtp_I2CBrief_t *response,
    uint8_t response_code,
    uint8_t exception_code,
    uint8_t response_data)
{
    Avtp_I2CBrief_CreateAcfMessage(
        response,
        response_code,
        false,
        Avtp_I2CBrief_GetTransactionNum(request),
        Avtp_I2CBrief_GetEvt(request),
        exception_code,
        response_data
    );

    Avtp_I2CBrief_SetI2CBusId(
        response,
        Avtp_I2CBrief_GetI2CBusId(request)
    );
}


/* Public dispatcher. */
int i2c_listener_process(
    const struct device *i2c_dev,
    i2c_message_format_t format,
    const void *request,
    size_t request_size,
    void *response,
    size_t response_size)
{
    if (request == NULL || response == NULL) {
        return -EINVAL;
    }

    switch (format) {
    case I2C_FORMAT_FULL:
        return process_full(
            i2c_dev,
            request,
            request_size,
            response,
            response_size
        );

    case I2C_FORMAT_BRIEF:
        return process_brief(
            i2c_dev,
            request,
            request_size,
            response,
            response_size
    );

    default:
        return -EINVAL;
    }
}