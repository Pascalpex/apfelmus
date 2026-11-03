#include <zephyr/sys/printk.h>
#include <string.h>

#include "../Open1722/include/avtp/acf/I2C.h"
#include "../Open1722/include/avtp/acf/I2CBrief.h"
#include "i2c_listener.h"

#define I2C_CODE_CR1  0
#define I2C_CODE_TR2  9

//creates an ACF message using the Avtp_I2C_CreateAcfMessage() method of the Open1722 library and calls the i2c_listener_process() to get a response. The response is then checked for correctness.
static void test_full_listener(void)
{
    Avtp_I2C_t request;
    Avtp_I2C_t response;

    printk("\n--- Test: Full ACF-I2C ---\n");

    /*
     * Create a CR1 request:
     * BH1750 address = 0x23
     * Read bit = 1
     * Data byte = (0x23 << 1) | 1 = 0x47
     */
    Avtp_I2C_CreateAcfMessage(
        &request,
        I2C_CODE_CR1,
        false,
        5,       /* Transaction number */
        0,       /* Event */
        0,       /* Exception code */
        0x47     /* BH1750 read request */
    );

    /* Give the request a test bus ID. */
    Avtp_I2C_SetI2CBusId(&request, 3);

    /* Pass the generated message to the listener. */
    int ret = i2c_listener_process(
        I2C_FORMAT_FULL,
        &request,
        sizeof(request),
        &response,
        sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: Listener returned %d\n", ret);
        return;
    }

    /* Check the fields decoded from the response. */
    printk("Response code: %u\n",
           Avtp_I2C_GetI2CCode(&response));

    printk("Transaction number: %u\n",
           Avtp_I2C_GetTransactionNum(&response));

    printk("Bus ID: %u\n",
           Avtp_I2C_GetI2CBusId(&response));

    if (Avtp_I2C_GetI2CCode(&response) == I2C_CODE_TR2 &&
        Avtp_I2C_GetTransactionNum(&response) == 5 &&
        Avtp_I2C_GetI2CBusId(&response) == 3) {
        printk("PASS: Full-format listener test\n");
    } else {
        printk("FAIL: Full-format response fields incorrect\n");
    }
}

static void test_brief_listener(void)
{
    Avtp_I2CBrief_t request;
    Avtp_I2CBrief_t response;

    printk("\n--- Test: Brief ACF-I2C ---\n");

    /* Create the same BH1750 read request in Brief format. */
    Avtp_I2CBrief_CreateAcfMessage(
        &request,
        I2C_CODE_CR1,
        false,
        7,       /* Transaction number */
        0,       /* Event */
        0,       /* Exception code */
        0x47     /* BH1750 read request */
    );

    Avtp_I2CBrief_SetI2CBusId(&request, 3);

    int ret = i2c_listener_process(
        I2C_FORMAT_BRIEF,
        &request,
        sizeof(request),
        &response,
        sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: Listener returned %d\n", ret);
        return;
    }

    printk("Response code: %u\n",
           Avtp_I2CBrief_GetI2CCode(&response));

    printk("Transaction number: %u\n",
           Avtp_I2CBrief_GetTransactionNum(&response));

    printk("Bus ID: %u\n",
           Avtp_I2CBrief_GetI2CBusId(&response));

    if (Avtp_I2CBrief_GetI2CCode(&response) == I2C_CODE_TR2 &&
        Avtp_I2CBrief_GetTransactionNum(&response) == 7 &&
        Avtp_I2CBrief_GetI2CBusId(&response) == 3) {
        printk("PASS: Brief-format listener test\n");
    } else {
        printk("FAIL: Brief-format response fields incorrect\n");
    }
}

// creates a raw ACF-I2C Brief message (8 bytes) and passes it to the listener, and checks the response for correctness.
 void test_raw_brief_message(void)
{
    printk("\n--- Testing raw ACF-I2C Brief message ---\n");

    /*
     * Manually prepared 8-byte ACF-I2C Brief message.
     *
     * ACF type       = 0x10 (I2C Brief)
     * ACF length     = 2 quadlets (8 bytes)
     * I2C bus ID     = 3
     * I2C code       = 0 (CR1: Start)
     * Transaction    = 7
     * Event          = 0
     * Exception      = 0
     * I2C data       = 0x47 (BH1750 address 0x23, read)
     */
    uint8_t raw_request[8] = {
        0x20, 0x02,
        0x00, 0x03,
        0x00, 0x07, 0x00, 0x47
    };

    Avtp_I2CBrief_t response;

    /* Pass the manually prepared bytes to your listener */
    int ret = i2c_listener_process(
        I2C_FORMAT_BRIEF,
        raw_request,
        sizeof(raw_request),
        &response,
        sizeof(response)
    );

    if (ret != 0) {
        printk("Raw Brief message processing failed: %d\n", ret);
        return;
    }

    /* Decode the response */
    printk("Response successfully created!\n");
    printk("Response I2C code: %u\n",
           Avtp_I2CBrief_GetI2CCode(&response));

    printk("Response transaction number: %u\n",
           Avtp_I2CBrief_GetTransactionNum(&response));

    printk("Response bus ID: %u\n",
           Avtp_I2CBrief_GetI2CBusId(&response));

    printk("Response exception code: %u\n",
           Avtp_I2CBrief_GetExceptionCode(&response));

    printk("Response I2C data: 0x%02X\n",
           Avtp_I2CBrief_GetI2CData(&response));
}

void test_full_i2c_message(void)
{
    printk("\n--- Testing full ACF-I2C message ---\n");

    /* Step 1: Create a message */
    Avtp_I2C_t message;

    Avtp_I2C_CreateAcfMessage(
        &message,
        0,      // CR1: Start
        false,  // TRR
        1,      // Transaction number
        0,      // Event
        0,      // Exception code
        0x20    // I2C data
    );

    /* Step 2: Check whether the message is valid */
    bool valid = Avtp_I2C_IsValid(&message, sizeof(message));

    printk("Message valid: %s\n", valid ? "YES" : "NO");

    if (!valid) {
        printk("Full I2C message validation failed!\n");
        return;
    }

    /* Step 3: Decode the message */
    uint8_t code = Avtp_I2C_GetI2CCode(&message);
    uint8_t transaction = Avtp_I2C_GetTransactionNum(&message);
    uint8_t data = Avtp_I2C_GetI2CData(&message);

    printk("Decoded I2C code: %u\n", code);
    printk("Decoded transaction number: %u\n", transaction);
    printk("Decoded I2C data: 0x%02X\n", data);

    /* Step 4: Check whether the decoded values match */
    if (code == 0 && transaction == 1 && data == 0x20) {
        printk("Full I2C decoding test PASSED!\n");
    } else {
        printk("Full I2C decoding test FAILED!\n");
    }
}


 void test_brief_i2c_message(void)
{
    printk("\n--- Testing ACF-I2C Brief message ---\n");

    /* Step 1: Create a Brief message */
    Avtp_I2CBrief_t message;

    Avtp_I2CBrief_CreateAcfMessage(
        &message,
        0,      // CR1: Start
        false,  // TRR
        1,      // Transaction number
        0,      // Event
        0,      // Exception code
        0x20    // I2C data
    );

    /* Step 2: Check whether the message is valid */
    bool valid = Avtp_I2CBrief_IsValid(&message, sizeof(message));

    printk("Brief message valid: %s\n", valid ? "YES" : "NO");

    if (!valid) {
        printk("Brief message validation failed!\n");
        return;
    }

    /* Step 3: Decode the message */
    uint8_t code = Avtp_I2CBrief_GetI2CCode(&message);
    uint8_t transaction = Avtp_I2CBrief_GetTransactionNum(&message);
    uint8_t data = Avtp_I2CBrief_GetI2CData(&message);

    printk("Decoded I2C code: %u\n", code);
    printk("Decoded transaction number: %u\n", transaction);
    printk("Decoded I2C data: 0x%02X\n", data);

    /* Step 4: Check whether the decoded values match */
    if (code == 0 && transaction == 1 && data == 0x20) {
        printk("Brief I2C decoding test PASSED!\n");
    } else {
        printk("Brief I2C decoding test FAILED!\n");
    }
}