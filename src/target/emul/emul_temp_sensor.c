/*
 * Emulated ADT7420 temperature sensor on the emulated I2C bus (emul.overlay).
 * Only built with I2C1722_EMUL=ON. Runs by itself at boot (SYS_INIT), main.c does not
 * know about it: it copies the CPU die temperature into the emulated sensor every 500 ms
 * and reads ID + temperature once as a self-test.
 */
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/emul_sensor.h>

LOG_MODULE_REGISTER(emul_temp_sensor, LOG_LEVEL_INF);

#define TEMP_SENSOR_NODE DT_NODELABEL(adt7420)
#define TEMP_SENSOR_REG_TEMP 0x00
#define TEMP_SENSOR_REG_ID 0x0B
#define DIE_TEMP_NODE DT_NODELABEL(die_temp)

static const struct device *die_temp = DEVICE_DT_GET(DIE_TEMP_NODE);
static const struct emul *temp_emul = EMUL_DT_GET(TEMP_SENSOR_NODE);
static struct k_work_delayable update_work;

/* Copies the CPU die temperature into the emulated ADT7420, every 500 ms */
static void update_temp(struct k_work *work)
{
    struct sensor_value val;
    struct sensor_chan_spec ch = {.chan_type = SENSOR_CHAN_AMBIENT_TEMP, .chan_idx = 0};

    if (sensor_sample_fetch(die_temp) == 0 &&
        sensor_channel_get(die_temp, SENSOR_CHAN_DIE_TEMP, &val) == 0)
    {
        /* q31 with shift 8: value = degC * 2^23 */
        q31_t q = (q31_t)(sensor_value_to_milli(&val) * (1 << 23) / 1000);
        emul_sensor_backend_set_channel(temp_emul, ch, &q, 8);
    }
    k_work_schedule(&update_work, K_MSEC(500));
}

static int emul_temp_sensor_init(void)
{
    const struct i2c_dt_spec sensor = I2C_DT_SPEC_GET(TEMP_SENSOR_NODE);

    if (!i2c_is_ready_dt(&sensor))
    {
        LOG_ERR("Emulated I2C bus not ready");
        return 0;
    }
    if (!device_is_ready(die_temp))
    {
        LOG_ERR("CPU temperature sensor not ready");
        return 0;
    }

    k_work_init_delayable(&update_work, update_temp);
    update_temp(NULL);

        uint8_t reg = TEMP_SENSOR_REG_ID, id = 0;
    int ret = i2c_write_read_dt(&sensor, &reg, 1, &id, 1);
    LOG_INF("Emulated ADT7420 @0x%02x id: ret=%d id=0x%02x", sensor.addr, ret, id);

    /* Temperature: 2 bytes from register 0x00, degC = raw / 128 */
    uint8_t raw[2] = {0};
    reg = TEMP_SENSOR_REG_TEMP;
    ret = i2c_write_read_dt(&sensor, &reg, 1, raw, 2);
    int16_t code = (int16_t)((raw[0] << 8) | raw[1]);
    LOG_INF("Emulated ADT7420 temp: ret=%d raw=0x%04x = %d mdegC", ret, (uint16_t)code,
            code * 1000 / 128);

    return 0;
}

SYS_INIT(emul_temp_sensor_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
