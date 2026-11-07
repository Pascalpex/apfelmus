OPEN1722 ?= ../../Open1722
CFLAGS   ?= -Wall -Wextra -O2
CFLAGS   += -I$(OPEN1722)/include -I../common

controller: main.c ../common/i2c1722.h
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f controller

.PHONY: clean
