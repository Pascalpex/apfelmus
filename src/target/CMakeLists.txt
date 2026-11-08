cmake_minimum_required(VERSION 3.20.0)

# Default network setup is DHCP (see prj.conf).

# I2C backend:
#   west build -b nucleo_f429zi                              -> emulated sensor (emul/)
#   west build -p -b nucleo_f429zi -- -DI2C1722_EMUL=OFF     -> real I2C on i2c1
option(I2C1722_EMUL "Use the emulated I2C bus + temp sensor from emul/" ON)
if(I2C1722_EMUL)
    list(APPEND EXTRA_DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/emul/emul.overlay)
    list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/emul/emul.conf)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(i2c1722_target)

set(OPEN1722_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Open1722 CACHE PATH "Open1722 checkout")

target_include_directories(app PRIVATE
    ${OPEN1722_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)
target_sources(app PRIVATE
    src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/i2c1722.c
)
if(I2C1722_EMUL)
    target_sources(app PRIVATE emul/emul_temp_sensor.c)
endif()
