#include "i2C_listener.h"
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

#define BH1750_ADDR 0x23

/**
 * @brief Stores the state and data of an ongoing I2C transaction.
 *
 * Keeps track of the peripheral address, read/write direction,
 * transaction number, bus ID, buffered write data and read position.
 */
typedef struct {
    bool active;
    uint8_t address;
    bool read;

    uint8_t write_data[34];
    size_t write_length;

    uint16_t fram_read_address;
    size_t read_count;

    uint8_t transaction;
    uint16_t bus_id;
} i2c_transaction_t;

/** Holds the state of the currently active ACF-I2C transaction. */
static i2c_transaction_t current_transaction;

// For the BH1750
static uint8_t bh1750_cached_data[2];
static bool bh1750_data_valid = false;

//definition
static void create_brief_response(
    const Avtp_I2CBrief_t *request,
    Avtp_I2CBrief_t *response,
    uint8_t response_code,
    uint8_t exception_code,
    uint8_t response_data);

/**
 * @brief Creates a Full-format ACF-I2C response message.
 *
 * Sets the specified response code, exception code and response data.
 * Copies the transaction number, event information and I2C bus ID
 * from the original request to ensure that the response belongs
 * to the correct transaction.
 */
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


/**
 * @brief Processes one Full-format ACF-I2C request.
 *
 * Validates the incoming message and processes its I2C command.
 * Handles transaction initialization (CR1), write-data collection
 * (CR3), read-address handling (CR5), write execution (CR4),
 * and data reading (CR6 and CR7).
 *
 * Supports FRAM read/write operations and the BH1750 measurement
 * and read sequence. Generates the corresponding ACF-I2C response.
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
        if (address != FRAM_ADDR && address != BH1750_ADDR) {
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
        current_transaction.read = read;
        current_transaction.transaction = transaction;
        current_transaction.bus_id = bus_id;
        current_transaction.read_count = 0;

        /* A BH1750 read is only allowed after a valid measurement. */
        if (address == BH1750_ADDR && read && !bh1750_data_valid) {
            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        /* CR1 accepted: return TR2 (ACK). */
        create_response(
            request,
            response,
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

            /* This BH1750 test sequence does not use CR5. */
        if (current_transaction.address == BH1750_ADDR) {
            current_transaction.active = false;

            create_response(
                request,
                response,
                8,      /* TR1: NACK */
                0,
                0
            );
            return 0;
        }

        uint8_t address_data = Avtp_I2C_GetI2CData(request);
        uint8_t address = address_data >> 1;
        bool read = (address_data & 0x01U) != 0U;

        if (address != FRAM_ADDR &&
            address != BH1750_ADDR) {

            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        current_transaction.fram_read_address =
            ((uint16_t)current_transaction.write_data[0] << 8) |
            current_transaction.write_data[1];

        current_transaction.read_count = 0;
        current_transaction.read = true;

        if (address == BH1750_ADDR && read) {
            if (!bh1750_data_valid) {
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
            current_transaction.read_count = 0;
        }

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

        if (current_transaction.address == BH1750_ADDR) {

            if (current_transaction.write_length != 1 ||
                current_transaction.write_data[0] != BH1750_ONETIME_HR) {

                current_transaction.active = false;

                create_response(
                    request, response,
                    I2C_CODE_TR1,
                    I2C_EXCEPTION_SEQUENCE,
                    0
                );

                return 0;
            }

            uint16_t raw_value;

            int ret = bh1750_read_raw(
                i2c_dev,
                &raw_value
            );

            current_transaction.active = false;

            if (ret != 0) {
                bh1750_data_valid = false;

                create_response(
                    request, response,
                    I2C_CODE_TR1,
                    I2C_EXCEPTION_NONE,
                    0
                );

                return ret;
            }

            bh1750_cached_data[0] =
                (uint8_t)(raw_value >> 8);

            bh1750_cached_data[1] =
                (uint8_t)(raw_value & 0xFF);

            bh1750_data_valid = true;

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

    /* CR6: Read one byte and continue. */
    if (code == 5) {

        if (current_transaction.address == BH1750_ADDR) {

            if (!current_transaction.read ||
                current_transaction.read_count >= 2) {
                return -EINVAL;
            }

            uint8_t read_data =
                bh1750_cached_data[current_transaction.read_count];

            current_transaction.read_count++;

            create_response(
                request, response,
                I2C_CODE_TR3,
                I2C_EXCEPTION_NONE,
                read_data
            );

            return 0;
        }

        if (!current_transaction.read) {
            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );
            return 0;
        }

        uint8_t read_data = 0;

        int ret = read_fram(
            i2c_dev,
            current_transaction.fram_read_address +
                current_transaction.read_count,
            &read_data,
            sizeof(read_data)
        );

        if (ret != 0) {
            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_NONE,
                0
            );

            return ret;
        }

        current_transaction.read_count++;

        create_response(
            request, response,
            I2C_CODE_TR3,
            I2C_EXCEPTION_NONE,
            read_data
        );

        return 0;
    }
    /* -------------------------------------------------
    * CR7: Read final byte, NACK and STOP.
    * ------------------------------------------------- */
    if (code == 6) {

        if (current_transaction.address == BH1750_ADDR) {

            if (!current_transaction.read ||
                current_transaction.read_count != 1) {
                current_transaction.active = false;
                return -EINVAL;
            }

            uint8_t read_data = bh1750_cached_data[1];

            current_transaction.read_count++;
            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR3,
                I2C_EXCEPTION_NONE,
                read_data
            );

            return 0;
        }
        if (!current_transaction.read) {
            current_transaction.active = false;

            create_response(
                request, response,
                I2C_CODE_TR1,
                I2C_EXCEPTION_SEQUENCE,
                0
            );

            return 0;
        }

        uint8_t read_data = 0;

        int ret = read_fram(
            i2c_dev,
            current_transaction.fram_read_address +
                current_transaction.read_count,
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


/**
 * @brief Processes one Brief-format ACF-I2C request.
 *
 * Validates the incoming Brief message and processes its I2C command.
 * Handles transaction initialization (CR1), write-data collection
 * (CR3), read-address handling (CR5), FRAM write execution (CR4),
 * and final-byte reading (CR7).
 *
 * Supports FRAM read and write operations and generates the
 * corresponding Brief-format ACF-I2C response.
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


/**
 * @brief Dispatches an ACF-I2C request to the appropriate message processor.
 *
 * Checks the requested message format and forwards Full-format
 * messages to process_full() or Brief-format messages to process_brief().
 * Returns an error if the request or response pointer is invalid
 * or the message format is unsupported.
 */
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