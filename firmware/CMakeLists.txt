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


# Enable GNU STD support.
include(${ZEPHYR_CONNECTEDHOMEIP_MODULE_DIR}/config/nrfconnect/app/enable-gnu-std.cmake)
# Include all source files that are located in the Matter common directory.
include(${ZEPHYR_NRF_MODULE_DIR}/samples/matter/common/cmake/source_common.cmake)
# Include Data Model utils
include(${ZEPHYR_NRF_MODULE_DIR}/samples/matter/common/cmake/data_model.cmake)
# Include ZAP helpers
include(${ZEPHYR_NRF_MODULE_DIR}/samples/matter/common/cmake/zap_helpers.cmake)
ncs_get_zap_parent_dir(ZAP_PARENT_DIR)

target_include_directories(app PRIVATE
    src/matter
    ${ZAP_PARENT_DIR}
)

ncs_configure_data_model()
