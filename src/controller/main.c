/*
 * Controller (laptop): accesses I2C peripherals on the target over IEEE 1722 (ACF_I2C).
 * The target only replays the bus primitives, all device knowledge lives here:
 *   udp_link.c  transport (1722 over UDP)
 *   fram.c      MB85RC256V FRAM
 *   bh1750.c    BH1750 light sensor
 *   mpu6050.c   MPU-6050 accelerometer/gyroscope
 *   oled.c      128x64 OLED display
 *   display.c   16x2 character LCD (HD44780 + PCF8574)
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "i2c1722.h"
#include "udp_link.h"
#include "fram.h"
#include "bh1750.h"
#include "mpu6050.h"
#include "oled.h"
#include "display.h"

#define FRAM_TEST_ADDR  0x0010 /* FRAM memory address used by the commands */
#define SENSOR_PERIOD_S 5      /* sensor polling period in seconds */

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

static void cmd_mpu6050(struct i2c1722_link *link)
{
    struct mpu6050_raw r;

    if (mpu6050_read_raw(link, &r) != 0)
    {
        printf("MPU6050 read FAILED\n");
        return;
    }
    printf("MPU6050: accel [g]   x=%6.2f y=%6.2f z=%6.2f\n",
           MPU6050_ACCEL_TO_G(r.accel[0]), MPU6050_ACCEL_TO_G(r.accel[1]), MPU6050_ACCEL_TO_G(r.accel[2]));
    printf("         gyro [°/s]  x=%6.1f y=%6.1f z=%6.1f\n",
           MPU6050_GYRO_TO_DPS(r.gyro[0]), MPU6050_GYRO_TO_DPS(r.gyro[1]), MPU6050_GYRO_TO_DPS(r.gyro[2]));
    printf("         temp        %.1f °C\n", MPU6050_TEMP_TO_C(r.temp));
}

static int oled_show(struct i2c1722_link *link, const char *text)
{
    if (strlen(text) > OLED_MAX_CHARS)
    {
        printf("OLED: text cut to %d characters\n", OLED_MAX_CHARS);
    }
    return oled_clear(link) || oled_print_centered(link, 3, text);
}

static int lcd_show(struct i2c1722_link *link, const char *text)
{
    return lcd_clear(link) || lcd_print(link, text);
}

/* Reads lines from stdin and passes them to show() until "q!" or EOF */
static void display_loop(struct i2c1722_link *link,
                         int (*show)(struct i2c1722_link *, const char *))
{
    char line[128];

    printf("Type text to display, \"q!\" to quit\n");
    for (;;)
    {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
        {
            break; /* EOF */
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "q!") == 0)
        {
            break;
        }
        if (show(link, line) != 0)
        {
            printf("write FAILED\n");
        }
    }
}

/* Calls read_sensor() every SENSOR_PERIOD_S seconds until Ctrl-C */
static void sensor_loop(struct i2c1722_link *link, void (*read_sensor)(struct i2c1722_link *))
{
    for (;;)
    {
        read_sensor(link);
        sleep(SENSOR_PERIOD_S);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <target-ip-or-hostname> "
                        "<oled | display | bh1750 | mpu6050 | fram_write [text] | fram_read [len]>\n",
                argv[0]);
        return 1;
    }
    const char *cmd = argv[2];
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
        sensor_loop(&link, cmd_bh1750);
    }
    else if (strcmp(cmd, "mpu6050") == 0)
    {
        sensor_loop(&link, cmd_mpu6050);
    }
    else if (strcmp(cmd, "oled") == 0)
    {
        if (oled_init(&link) != 0)
        {
            printf("OLED init FAILED (check address 0x%02X / wiring)\n", OLED_ADDR);
        }
        else
        {
            display_loop(&link, oled_show);
        }
    }
    else if (strcmp(cmd, "display") == 0)
    {
        if (lcd_init(&link) != 0)
        {
            printf("LCD init FAILED\n");
        }
        else
        {
            display_loop(&link, lcd_show);
        }
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", cmd);
    }

    udp_link_close(&link);
    return 0;
}
