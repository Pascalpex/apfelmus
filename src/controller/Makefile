OPEN1722 ?= ../../Open1722
CFLAGS   ?= -Wall -Wextra -O2
CFLAGS   += -I$(OPEN1722)/include -I../common

SRCS = main.c udp_link.c fram.c bh1750.c mpu6050.c oled.c display.c ../common/i2c1722.c
HDRS = udp_link.h fram.h bh1750.h mpu6050.h oled.h display.h ../common/i2c1722.h

controller: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f controller

.PHONY: clean
