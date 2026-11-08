OPEN1722 ?= ../../Open1722
CFLAGS   ?= -Wall -Wextra -O2
CFLAGS   += -I$(OPEN1722)/include -I../common

controller: main.c ../common/i2c1722.c ../common/i2c1722.h ../common/i2c1722_proto.h
	$(CC) $(CFLAGS) main.c ../common/i2c1722.c -o $@

clean:
	rm -f controller

.PHONY: clean
