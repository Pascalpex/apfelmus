#include "i2c_listener.h"

#define I2C_CODE_CR1  0
#define I2C_CODE_TR1  8
#define I2C_CODE_TR2  9

#define FRAM_ADDR     0x50
#define BH1750_ADDR   0x23

typedef struct {
    bool active;
    uint8_t address;
    uint8_t data[32];
    size_t data_length;
    uint8_t transaction;
    uint16_t bus_id;
} i2c_transaction_t;

static i2c_transaction_t current_transaction;

// Receive a full ACF-I²C message, decode it, and create a response message.
//request	Pointer to the incoming ACF-I²C message
//request_size	Number of bytes available in that message
//response	Memory where the function will build its reply
//response_size	Amount of memory available for the reply
static int process_full(
    const Avtp_I2C_t *request,
    size_t request_size,
    Avtp_I2C_t *response,
    size_t response_size)
{
    if (response_size < sizeof(*response) ||
        !Avtp_I2C_IsValid(request, request_size)) {
        return -1;
    }

    uint8_t code = Avtp_I2C_GetI2CCode(request); //which I2C operation is requested (e.g. read, write, etc.)
    uint8_t transaction = Avtp_I2C_GetTransactionNum(request); //number that gets incremented with each request, used to find messages lost in transit (response message shall have the same transaction number as the request)
    uint16_t bus_id = Avtp_I2C_GetI2CBusId(request); //which I2C bus is used-the response message shall have the same bus ID as the request

    if (code != I2C_CODE_CR1) {
        return -2;  /* Request code not implemented yet */
    }

    uint8_t data = Avtp_I2C_GetI2CData(request); //for the start and restart: the 7-bit device target address is carried in the upper 7 bits (bits 7:1) and the Read/Write control is carried in bit 0 (read=1, write=0) otherwise, the data byte is carried in bits 7:0 of the I2C data field
    uint8_t address = data >> 1; //to remove the bit 0 (read/write) and get the 7-bit address

    bool supported =
        (address == FRAM_ADDR) ||
        (address == BH1750_ADDR);

    uint8_t response_code = supported ? I2C_CODE_TR2 : I2C_CODE_TR1; //if supported choose response code TR2 (ACK), otherwise TR1 (NACK)
    uint8_t exception_code = 0; // No exception

    Avtp_I2C_CreateAcfMessage(
        response,
        response_code,
        false, //TRR is cleared
        transaction,
        0,
        exception_code,
        0 //I2C data byte
    );

    Avtp_I2C_SetI2CBusId(response, bus_id);

    return 0;
}

static int process_brief(
    const Avtp_I2CBrief_t *request,
    size_t request_size,
    Avtp_I2CBrief_t *response,
    size_t response_size)
{
    if (response_size < sizeof(*response) ||
        !Avtp_I2CBrief_IsValid(request, request_size)) {
        return -1;
    }

    uint8_t code = Avtp_I2CBrief_GetI2CCode(request);
    uint8_t transaction = Avtp_I2CBrief_GetTransactionNum(request);
    uint16_t bus_id = Avtp_I2CBrief_GetI2CBusId(request);

    if (code != I2C_CODE_CR1) {
        return -2;
    }

    uint8_t data = Avtp_I2CBrief_GetI2CData(request);
    uint8_t address = data >> 1;

    bool supported =
        (address == FRAM_ADDR) ||
        (address == BH1750_ADDR);

    uint8_t response_code = supported ? I2C_CODE_TR2 : I2C_CODE_TR1;
     uint8_t exception_code = 0;

    Avtp_I2CBrief_CreateAcfMessage(
        response,
        response_code,
        false,
        transaction,
        0,
        exception_code,
        0
    );

    Avtp_I2CBrief_SetI2CBusId(response, bus_id);

    return 0;
}

// Decides based on the given format whether to process a full or brief ACF-I²C message, and calls the appropriate function.
int i2c_listener_process(
    const struct device *i2c_dev,
    i2c_message_format_t format,
    const void *request,
    size_t request_size,
    void *response,
    size_t response_size)
{
    if (request == NULL || response == NULL) {
        return -1;
    } // Check for NULL pointers

    switch (format) {
    case I2C_FORMAT_FULL:
        return process_full(request, request_size, response, response_size);

    case I2C_FORMAT_BRIEF:
        return process_brief(request, request_size, response, response_size);

    default:
        return -1;
    }
}