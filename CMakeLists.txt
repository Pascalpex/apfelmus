cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(acf_i2c)

target_sources(app PRIVATE
    src/main.c
    src/fram.c
    src/bh1750.c
    src/i2C_listener.c
    src/i2C_test_listener.c
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Open1722/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)