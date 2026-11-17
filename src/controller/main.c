/*
 * Controller (laptop): accesses I2C peripherals on the target over IEEE 1722 (ACF_I2C).
 * The target only replays the bus primitives, all device knowledge lives here:
 *   udp_link.c  transport (1722 over UDP)
 *   fram.c      MB85RC256V FRAM
 *   bh1750.c    BH1750 light sensor
 *   oled.c      128x64 OLED display
 *   display.c   16x2 character LCD (HD44780 + PCF8574)
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2c1722.h"
#include "udp_link.h"
#include "fram.h"
#include "bh1750.h"
#include "oled.h"
#include "display.h"

#define FRAM_TEST_ADDR 0x0010 /* FRAM memory address used by the commands */

static void cmd_fram_write(struct i2c1722_link *link, const char *text)
{
    size_t len = strlen(text);
    int ret = fram_write(link, FRAM_TEST_ADDR, (const uint8_t *)text, len);
    printf("FRAM write \"%s\" (%zu bytes) @0x%04x: %s\n", text, len, FRAM_TEST_ADDR,
           ret == 0 ? "OK" : "FAILED");
}

static void cmd_fram_read(struct i2c1722_link *link, size_t len)
{
    uint8_t data[256];

    if (len == 0 || len > sizeof(data))
    {
        printf("FRAM read: length must be 1..%zu\n", sizeof(data));
        return;
    }
    if (fram_read(link, FRAM_TEST_ADDR, data, len) != 0)
    {
        printf("FRAM read FAILED\n");
        return;
    }
    printf("FRAM read %zu bytes @0x%04x:", len, FRAM_TEST_ADDR);
    for (size_t i = 0; i < len; i++)
    {
        printf(" %02x", data[i]);
    }
    printf("  \"");
    for (size_t i = 0; i < len; i++)
    {
        putchar(isprint(data[i]) ? data[i] : '.');
    }
    printf("\"\n");
}

static void cmd_bh1750(struct i2c1722_link *link)
{
    uint16_t raw;

    if (bh1750_read_raw(link, &raw) != 0)
    {
        printf("BH1750 read FAILED\n");
        return;
    }
    printf("BH1750: raw=%u -> %.1f lx\n", raw, BH1750_RAW_TO_LUX(raw));
}

static void cmd_oled(struct i2c1722_link *link, const char *text)
{
    if (oled_init(link) != 0)
    {
        printf("OLED init FAILED (check address 0x%02X / wiring)\n", OLED_ADDR);
        return;
    }
    if (strlen(text) > OLED_MAX_CHARS)
    {
        printf("OLED: text cut to %d characters\n", OLED_MAX_CHARS);
    }
    if (oled_print_centered(link, 3, text) != 0)
    {
        printf("OLED: write FAILED\n");
        return;
    }
    printf("OLED: \"%.*s\" written\n", OLED_MAX_CHARS, text);
}

static void cmd_display(struct i2c1722_link *link, const char *text)
{
    if (lcd_init(link) != 0 || lcd_print(link, text) != 0)
    {
        printf("LCD write FAILED\n");
        return;
    }
    printf("LCD: \"%s\" written\n", text);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <target-ip-or-hostname> "
                        "[fram_write [text] | fram_read [len] | bh1750 | oled [text] | display [text]]\n",
                argv[0]);
        return 1;
    }
    const char *cmd = argc > 2 ? argv[2] : "oled";
    const char *arg = argc > 3 ? argv[3] : NULL;

    struct i2c1722_link link;
    if (udp_link_open(&link, argv[1]) != 0)
    {
        return 1;
    }

    if (strcmp(cmd, "fram_write") == 0)
    {
        cmd_fram_write(&link, arg ? arg : "HELLO");
    }
    else if (strcmp(cmd, "fram_read") == 0)
    {
        cmd_fram_read(&link, arg ? (size_t)strtoul(arg, NULL, 0) : 5);
    }
    else if (strcmp(cmd, "bh1750") == 0)
    {
        cmd_bh1750(&link);
    }
    else if (strcmp(cmd, "oled") == 0)
    {
        cmd_oled(&link, arg ? arg : "HELLO WORLD!");
    }
    else if (strcmp(cmd, "display") == 0)
    {
        cmd_display(&link, arg ? arg : "HELLO WORLD!");
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", cmd);
    }

    udp_link_close(&link);
    return 0;
}
