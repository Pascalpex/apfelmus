# Host unit tests (Linux/WSL). Needs cmocka: sudo apt install libcmocka-dev
#   make        build the tests
#   make test   build and run them

OPEN1722 ?= ../Open1722
CFLAGS   ?= -std=c99 -Wall -Wextra -Wconversion -Wsign-conversion -Wfatal-errors
CFLAGS   += -I$(OPEN1722)/include -I../src/common
# cmocka test functions must take "void **state" even if they do not use it
CFLAGS   += -Wno-unused-parameter
LDLIBS   += -lcmocka

TESTS = test_i2c_codes

all: $(TESTS)

test_i2c_codes: test_i2c_codes.c ../src/common/avtp_i2c_codes.h
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

clean:
	rm -f $(TESTS)

.PHONY: all test clean
