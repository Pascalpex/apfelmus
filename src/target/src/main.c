/* Target Controller (board): executes received ACF_I2C requests on the local I2C bus. */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "i2c1722.h"

LOG_MODULE_REGISTER(i2c1722_target, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("i2c1722 target");

    /* TODO */
    return 0;
}
