# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(air_quality_sensor)

# Add the header files
include_directories(
    include 
    thirdparty/sensirion_gas_index_algorithm
)

FILE(GLOB_RECURSE app_sources
    src/*.c
    src/*.cpp
    thirdparty/*.c
)

target_sources(app PRIVATE ${app_sources})

# Matter over Thread (see src/matter/) is the only supported communication
# stack - src/main.c and air_quality_monitor.c are the single entry
# point/periodic task, with components/matter_handler.cpp driving Matter
# server startup and cluster attribute updates.

# Enable GNU STD support and initialize the Matter Data Model, matching
# the pattern used by NCS's own Matter samples.
include(${ZEPHYR_CONNECTEDHOMEIP_MODULE_DIR}/config/nrfconnect/app/enable-gnu-std.cmake)
include(${ZEPHYR_CONNECTEDHOMEIP_MODULE_DIR}/src/app/chip_data_model.cmake)
include(${ZEPHYR_NRF_MODULE_DIR}/samples/matter/common/cmake/source_common.cmake)

target_include_directories(app PRIVATE
    src/matter
    ${CONFIG_NCS_SAMPLE_MATTER_ZAP_FILES_PATH}
)

chip_configure_data_model(app
    INCLUDE_SERVER
    BYPASS_IDL
    GEN_DIR ${CONFIG_NCS_SAMPLE_MATTER_ZAP_FILES_PATH}/zap-generated
    ZAP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_NCS_SAMPLE_MATTER_ZAP_FILES_PATH}/aqs.zap
)
