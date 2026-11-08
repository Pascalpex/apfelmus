#include <zephyr/sys/printk.h>
#include <string.h>

#include "../Open1722/include/avtp/acf/I2C.h"
#include "../Open1722/include/avtp/acf/I2CBrief.h"
#include "i2c_listener.h"
#include "i2c_test_listener.h"

#define I2C_CODE_CR1  0
#define I2C_CODE_TR2  9
#define I2C_CODE_TR3  10
#define I2C_CODE_TR5  12

#define TEST_FRAM_ADDRESS 0x0100
#define TEST_FRAM_VALUE   0xA5

void test_full_fram_write(const struct device *i2c_dev)
{
    printk("\n--- Test: ACF-I2C Full FRAM write ---\n");

    Avtp_I2C_t request;
    Avtp_I2C_t response;

    int ret;
    uint8_t transaction = 42;
    uint16_t bus_id = 3;

    /*
     * CR1: START
     * FRAM address = 0x50
     * Write bit = 0
     * I2C data = (0x50 << 1) = 0xA0
     */
    Avtp_I2C_CreateAcfMessage(
        &request, 0, false, transaction, 0, 0, 0xA0
    ); //*pdu, i2c_code, trr (target response request)-0: Target Controller does not send response, transaction_num, event,exception_code, i2c_data)

    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process( //checks whether the request is valid and processes it accordingly, returning a response.
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: CR1 START\n");
        return;
    }

    /*
     * CR3: Send high byte of FRAM memory address.
     */
    Avtp_I2C_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0, 0x01
    ); //*pdu, i2c_code, trr (target response request)-0: Target Controller does not send response, transaction_num, event,exception_code, i2c_data)

    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: CR3 address high byte\n");
        return;
    }

    /*
     * CR3: Send low byte of FRAM memory address.
     */
    Avtp_I2C_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0, 0x00
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR3 address low byte\n");
        return;
    }

    /*
     * CR3: Send the actual value to store.
     */
    Avtp_I2C_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0,
        TEST_FRAM_VALUE
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR3 data byte\n");
        return;
    }

    /*
     * CR4: STOP
     * TRR = true, so request STOP confirmation.
     */
    Avtp_I2C_CreateAcfMessage(
        &request, 3, true, transaction, 0, 0, 0
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR4 STOP / FRAM write\n");
        return;
    }

    if (Avtp_I2C_GetI2CCode(&response) != 12) {
        printk("FAIL: Expected TR5 STOP confirmation\n");
        return;
    }

    printk("ACF-I2C write sequence completed.\n");

    /*
     * Verify the physical FRAM contents independently.
     */
    uint8_t read_value = 0;

    ret = read_fram(
        i2c_dev,
        TEST_FRAM_ADDRESS,
        &read_value,
        sizeof(read_value)
    );

    if (ret != 0) {
        printk("FAIL: Could not read FRAM back: %d\n", ret);
        return;
    }

    printk("FRAM value read back: 0x%02X\n", read_value);

    if (read_value == TEST_FRAM_VALUE) {
        printk("PASS: ACF-I2C FRAM write verified!\n");
    } else {
        printk("FAIL: Expected 0x%02X, got 0x%02X\n",
               TEST_FRAM_VALUE, read_value);
    }
}



void test_brief_fram_write(const struct device *i2c_dev)
{
    printk("\n--- Test: ACF-I2C Brief FRAM write ---\n");

    Avtp_I2CBrief_t request;
    Avtp_I2CBrief_t response;

    int ret;
    uint8_t transaction = 43;
    uint16_t bus_id = 3;

    /* Use a different FRAM address from the Full-format test. */
    const uint16_t test_address = 0x0101;
    const uint8_t test_value = 0x5A;

    /*
     * CR1: START
     * FRAM address 0x50, write bit 0 -> 0xA0
     */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 0, false, transaction, 0, 0, 0xA0
    );
    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2CBrief_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: Brief CR1 START\n");
        return;
    }

    /* CR3: FRAM memory address high byte. */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0, 0x01
    );
    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2CBrief_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: Brief CR3 address high byte\n");
        return;
    }

    /* CR3: FRAM memory address low byte. */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0, 0x01
    );
    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2CBrief_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: Brief CR3 address low byte\n");
        return;
    }

    /* CR3: Actual data byte. */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0, test_value
    );
    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2CBrief_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: Brief CR3 data byte\n");
        return;
    }

    /*
     * CR4: STOP
     * TRR = true, so request STOP confirmation.
     */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 3, true, transaction, 0, 0, 0
    );
    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2CBrief_GetI2CCode(&response) != I2C_CODE_TR5) {
        printk("FAIL: Brief CR4 STOP / FRAM write\n");
        return;
    }

    printk("Brief ACF-I2C write sequence completed.\n");

    /* Verify the physical FRAM contents. */
    uint8_t read_value = 0;

    ret = read_fram(
        i2c_dev,
        test_address,
        &read_value,
        sizeof(read_value)
    );

    if (ret != 0) {
        printk("FAIL: Could not read FRAM back: %d\n", ret);
        return;
    }

    printk("Brief FRAM value read back: 0x%02X\n", read_value);

    if (read_value == test_value) {
        printk("PASS: Brief ACF-I2C FRAM write verified!\n");
    } else {
        printk("FAIL: Expected 0x%02X, got 0x%02X\n",
               test_value, read_value);
    }
}


void test_brief_fram_read(const struct device *i2c_dev)
{
    printk("\n--- Test: ACF-I2C Brief FRAM read ---\n");

    Avtp_I2CBrief_t request;
    Avtp_I2CBrief_t response;

    int ret;
    uint8_t transaction = 45;
    uint16_t bus_id = 3;

    uint8_t expected_value = 0x7C;

    /* Prepare known data in the physical FRAM. */
    ret = write_to_fram(
        i2c_dev,
        TEST_FRAM_ADDRESS,
        &expected_value,
        sizeof(expected_value)
    );

    if (ret != 0) {
        printk("FAIL: Could not prepare FRAM test data\n");
        return;
    }

    /*
     * CR1: START
     * FRAM address 0x50 + WRITE = 0xA0
     */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 0, false, transaction, 0, 0, 0xA0
    );

    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2CBrief_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: CR1 START\n");
        return;
    }

    /*
     * CR3: Memory address high byte.
     */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0,
        (uint8_t)(TEST_FRAM_ADDRESS >> 8)
    );

    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR3 address high byte\n");
        return;
    }

    /*
     * CR3: Memory address low byte.
     */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0,
        (uint8_t)(TEST_FRAM_ADDRESS & 0xFF)
    );

    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR3 address low byte\n");
        return;
    }

    /*
     * CR5: Repeated START
     * FRAM address 0x50 + READ = 0xA1
     */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 4, false, transaction, 0, 0, 0xA1
    );

    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2CBrief_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: CR5 repeated START\n");
        return;
    }

    /*
     * CR7: Read final byte, NACK and STOP.
     */
    Avtp_I2CBrief_CreateAcfMessage(
        &request, 6, false, transaction, 0, 0, 0
    );

    Avtp_I2CBrief_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_BRIEF,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2CBrief_GetI2CCode(&response) != I2C_CODE_TR3) {
        printk("FAIL: CR7 read response\n");
        return;
    }

    uint8_t received_value =
        Avtp_I2CBrief_GetI2CData(&response);

    printk("FRAM value received through Brief ACF-I2C: 0x%02X\n",
           received_value);

    if (received_value == expected_value) {
        printk("PASS: Brief ACF-I2C FRAM read verified!\n");
    } else {
        printk("FAIL: Expected 0x%02X, got 0x%02X\n",
               expected_value, received_value);
    }
}


void test_full_fram_read(const struct device *i2c_dev)
{
    printk("\n--- Test: ACF-I2C Full FRAM read ---\n");

    Avtp_I2C_t request;
    Avtp_I2C_t response;

    int ret;
    uint8_t transaction = 44;
    uint16_t bus_id = 3;

    /* First, write a known value directly to the FRAM. */
    uint8_t expected_value = 0x6B;

    ret = write_to_fram(
        i2c_dev,
        TEST_FRAM_ADDRESS,
        &expected_value,
        sizeof(expected_value)
    );

    if (ret != 0) {
        printk("FAIL: Could not prepare FRAM test data\n");
        return;
    }

    /* CR1: START, FRAM address + WRITE (0xA0). */
    Avtp_I2C_CreateAcfMessage(
        &request, 0, false, transaction, 0, 0, 0xA0
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: CR1 START\n");
        return;
    }

    /* CR3: Memory address high byte. */
    Avtp_I2C_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0,
        (uint8_t)(TEST_FRAM_ADDRESS >> 8)
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR3 address high byte\n");
        return;
    }

    /* CR3: Memory address low byte. */
    Avtp_I2C_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0,
        (uint8_t)(TEST_FRAM_ADDRESS & 0xFF)
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR3 address low byte\n");
        return;
    }

    /* CR5: Repeated START, FRAM address + READ (0xA1). */
    Avtp_I2C_CreateAcfMessage(
        &request, 4, false, transaction, 0, 0, 0xA1
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: CR5 repeated START\n");
        return;
    }

    /* CR7: Read final byte, NACK and STOP. */
    Avtp_I2C_CreateAcfMessage(
        &request, 6, false, transaction, 0, 0, 0
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR3) {
        printk("FAIL: CR7 read response\n");
        return;
    }

    uint8_t received_value =
        Avtp_I2C_GetI2CData(&response);

    printk("FRAM value received through ACF-I2C: 0x%02X\n",
           received_value);

    if (received_value == expected_value) {
        printk("PASS: ACF-I2C FRAM read verified!\n");
    } else {
        printk("FAIL: Expected 0x%02X, got 0x%02X\n",
               expected_value, received_value);
    }
}


void test_full_fram_multibyte_read(const struct device *i2c_dev)
{
    printk("\n--- Test: Full ACF-I2C multi-byte FRAM read ---\n");

    Avtp_I2C_t request;
    Avtp_I2C_t response;

    const uint8_t transaction = 46;
    const uint16_t bus_id = 3;
    const uint16_t test_address = 0x0200;

    const uint8_t expected[3] = {0x11, 0x22, 0x33};
    uint8_t received[3] = {0};

    int ret;

    /* Prepare three known bytes in FRAM. */
    ret = write_to_fram(
        i2c_dev,
        test_address,
        expected,
        sizeof(expected)
    );

    if (ret != 0) {
        printk("FAIL: Could not prepare FRAM data\n");
        return;
    }

    /* CR1: START + FRAM WRITE. */
    Avtp_I2C_CreateAcfMessage(
        &request, 0, false, transaction, 0, 0, 0xA0
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: CR1\n");
        return;
    }

    /* CR3: Memory address high byte. */
    Avtp_I2C_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0, 0x02
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR3 address high\n");
        return;
    }

    /* CR3: Memory address low byte. */
    Avtp_I2C_CreateAcfMessage(
        &request, 2, false, transaction, 0, 0, 0x00
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0) {
        printk("FAIL: CR3 address low\n");
        return;
    }

    /* CR5: Repeated START + FRAM READ. */
    Avtp_I2C_CreateAcfMessage(
        &request, 4, false, transaction, 0, 0, 0xA1
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: CR5\n");
        return;
    }

    /* CR6: Read byte 1, continue. */
    for (size_t i = 0; i < 2; i++) {

        Avtp_I2C_CreateAcfMessage(
            &request, 5, false, transaction, 0, 0, 0
        );
        Avtp_I2C_SetI2CBusId(&request, bus_id);

        ret = i2c_listener_process(
            i2c_dev, I2C_FORMAT_FULL,
            &request, sizeof(request),
            &response, sizeof(response)
        );

        if (ret != 0 ||
            Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR3) {
            printk("FAIL: CR6 read byte %u\n", (unsigned int)i);
            return;
        }

        received[i] = Avtp_I2C_GetI2CData(&response);
    }

    /* CR7: Read byte 3, NACK + STOP. */
    Avtp_I2C_CreateAcfMessage(
        &request, 6, false, transaction, 0, 0, 0
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR3) {
        printk("FAIL: CR7 read byte 3\n");
        return;
    }

    received[2] = Avtp_I2C_GetI2CData(&response);

    printk("Received: %02X %02X %02X\n",
           received[0], received[1], received[2]);

    if (memcmp(received, expected, sizeof(expected)) == 0) {
        printk("PASS: Full ACF-I2C multi-byte read verified!\n");
    } else {
        printk("FAIL: Multi-byte FRAM data mismatch\n");
    }
}


//creates an ACF message using the Avtp_I2C_CreateAcfMessage() method of the Open1722 library and calls the i2c_listener_process() to get a response. The response is then checked for correctness.

void test_full_listener(const struct device *i2c_dev)
{
    printk("\n--- Test: Full ACF-I2C ---\n");

    Avtp_I2C_t request;
    Avtp_I2C_t response;

    uint8_t transaction = 5;
    uint16_t bus_id = 3;

    /*
     * CR1: START
     * FRAM address = 0x50
     * Write bit = 0
     * I2C data = 0xA0
     */
    Avtp_I2C_CreateAcfMessage(
        &request, 0, false, transaction, 0, 0, 0xA0
    );

    Avtp_I2C_SetI2CBusId(&request, bus_id);

    int ret = i2c_listener_process(
        i2c_dev,
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

    uint8_t response_code = Avtp_I2C_GetI2CCode(&response);

    printk("Response code: %u\n", response_code);
    printk("Transaction number: %u\n",
           Avtp_I2C_GetTransactionNum(&response));
    printk("Bus ID: %u\n",
           Avtp_I2C_GetI2CBusId(&response));

    if (response_code != I2C_CODE_TR2 ||
        Avtp_I2C_GetTransactionNum(&response) != transaction ||
        Avtp_I2C_GetI2CBusId(&response) != bus_id) {
        printk("FAIL: Full-format response fields incorrect\n");
        return;
    }

    printk("PASS: Full-format listener test!\n");
}


void test_brief_listener(const struct device *i2c_dev)
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
        i2c_dev,
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
 void test_raw_brief_message(const struct device *i2c_dev)
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
       i2c_dev,
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

    if (Avtp_I2CBrief_GetI2CCode(&response) == I2C_CODE_TR2 &&
        Avtp_I2CBrief_GetTransactionNum(&response) == 7 &&
        Avtp_I2CBrief_GetI2CBusId(&response) == 3) {
        printk("PASS: Raw Brief message test\n");
    } else {
        printk("FAIL: Raw Brief response incorrect\n");
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

void test_full_bh1750_read(const struct device *i2c_dev)
{
    printk("\n--- Test: Full ACF-I2C BH1750 read ---\n");

    Avtp_I2C_t request;
    Avtp_I2C_t response;

    const uint16_t bus_id = 3;
    const uint8_t write_transaction = 50;
    const uint8_t read_transaction = 51;

    uint8_t msb;
    uint8_t lsb;
    uint16_t raw;
    float lux;

    int ret;

    /* ---------------------------------------
     * PHASE 1: Start a BH1750 measurement
     * --------------------------------------- */

    /* CR1: START + BH1750 address + WRITE (0x46). */
    Avtp_I2C_CreateAcfMessage(
        &request, 0, false,
        write_transaction, 0, 0, 0x46
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: BH1750 write START\n");
        return;
    }

    printk("BH1750 write address accepted.\n");

    /* CR3: Send one-time high-resolution command (0x20). */
    Avtp_I2C_CreateAcfMessage(
        &request, 2, false,
        write_transaction, 0, 0, 0x20
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: BH1750 measurement command\n");
        return;
    }

    printk("BH1750 measurement command accepted.\n");

    /* CR4: STOP, request STOP confirmation (TRR = true). */
    Avtp_I2C_CreateAcfMessage(
        &request, 3, true,
        write_transaction, 0, 0, 0
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR5) {
        printk("FAIL: BH1750 measurement / STOP\n");
        return;
    }

    printk("BH1750 measurement completed.\n");

    /* ---------------------------------------
     * PHASE 2: Read the two sensor bytes
     * --------------------------------------- */

    /* CR1: START + BH1750 address + READ (0x47). */
    Avtp_I2C_CreateAcfMessage(
        &request, 0, false,
        read_transaction, 0, 0, 0x47
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR2) {
        printk("FAIL: BH1750 read START\n");
        return;
    }

    printk("BH1750 read address accepted.\n");

    /* CR6: Read first byte and continue. */
    Avtp_I2C_CreateAcfMessage(
        &request, 5, false,
        read_transaction, 0, 0, 0
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR3) {
        printk("FAIL: BH1750 CR6\n");
        return;
    }

    msb = Avtp_I2C_GetI2CData(&response);

    /* CR7: Read final byte, NACK and STOP. */
    Avtp_I2C_CreateAcfMessage(
        &request, 6, false,
        read_transaction, 0, 0, 0
    );
    Avtp_I2C_SetI2CBusId(&request, bus_id);

    ret = i2c_listener_process(
        i2c_dev, I2C_FORMAT_FULL,
        &request, sizeof(request),
        &response, sizeof(response)
    );

    if (ret != 0 ||
        Avtp_I2C_GetI2CCode(&response) != I2C_CODE_TR3) {
        printk("FAIL: BH1750 CR7\n");
        return;
    }

    lsb = Avtp_I2C_GetI2CData(&response);

    /* ---------------------------------------
     * PHASE 3: Convert and display the result
     * --------------------------------------- */

    raw = ((uint16_t)msb << 8) | lsb;
    lux = (float)raw / 1.2f;

    uint32_t lux_x100 = ((uint32_t)raw * 250U + 1U) / 3U;

    printk("BH1750 MSB: 0x%02X\n", msb);
    printk("BH1750 LSB: 0x%02X\n", lsb);
    printk("Raw sensor value: %u\n", raw);
    printk("Light level: %u.%02u lux\n",
       (unsigned int)(lux_x100 / 100U),
       (unsigned int)(lux_x100 % 100U));

    printk("PASS: Full ACF-I2C BH1750 read verified!\n");
}