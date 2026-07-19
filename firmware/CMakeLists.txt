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
    thirdparty/*.c
)

if(CONFIG_CHIP)
    # Matter over Thread build (see src/matter/): the custom BLE GATT
    # peripheral is replaced by the Matter (CHIP) application, which manages
    # its own BLE commissioning and Thread networking.
    #
    # air_quality_monitor.c is also excluded for now - this first bring-up
    # task only proves the bare Matter+Thread commissioning pipeline works.
    # Wiring sensors into Matter attributes is a follow-up
    # task, at which point air_quality_monitor.c (or an equivalent) will be
    # driven from src/matter/app_task.cpp instead of src/main.c.
    list(REMOVE_ITEM app_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/air_quality_monitor.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/components/bluetooth_handler.c
    )
    FILE(GLOB_RECURSE ble_service_sources src/ble_services/*.c)
    list(REMOVE_ITEM app_sources ${ble_service_sources})
endif()

target_sources(app PRIVATE ${app_sources})

if(CONFIG_CHIP)
    # Enable GNU STD support and initialize the Matter Data Model, matching
    # the pattern used by NCS's own Matter samples.
    include(${ZEPHYR_CONNECTEDHOMEIP_MODULE_DIR}/config/nrfconnect/app/enable-gnu-std.cmake)
    include(${ZEPHYR_CONNECTEDHOMEIP_MODULE_DIR}/src/app/chip_data_model.cmake)
    include(${ZEPHYR_NRF_MODULE_DIR}/samples/matter/common/cmake/source_common.cmake)

    target_include_directories(app PRIVATE
        src/matter
        ${CONFIG_NCS_SAMPLE_MATTER_ZAP_FILES_PATH}
    )

    target_sources(app PRIVATE
        src/matter/main.cpp
        src/matter/app_task.cpp
    )

    chip_configure_data_model(app
        INCLUDE_SERVER
        BYPASS_IDL
        GEN_DIR ${CONFIG_NCS_SAMPLE_MATTER_ZAP_FILES_PATH}/zap-generated
        ZAP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_NCS_SAMPLE_MATTER_ZAP_FILES_PATH}/aqs.zap
    )
endif()
