cmake_minimum_required(VERSION 3.20.0)

# Default network setup is DHCP (see prj.conf).

# I2C backend:
#   west build -b nucleo_f429zi

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(i2c1722_target)

set(OPEN1722_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Open1722 CACHE PATH "Open1722 checkout")

target_include_directories(app PRIVATE
    ${OPEN1722_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)
target_sources(app PRIVATE
    src/main.c
    src/i2C_listener.c
    src/fram.c
    src/bh1750.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/i2c1722.c
)

