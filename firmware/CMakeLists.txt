# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(air-quality-sensor-nrf52)

# Add the header files
include_directories(
    include 
    thirdparty/sensirion_gas_index_algorithm
)

FILE(GLOB_RECURSE app_sources
    src/*.c
    thirdparty/*.c
)
target_sources(app PRIVATE ${app_sources})
