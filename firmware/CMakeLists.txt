# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(air_quality_sensor)

target_include_directories(app PRIVATE
    include
    thirdparty/sensirion_gas_index_algorithm
)

# Listed explicitly rather than globbed: a glob is evaluated at configure time,
# so new files are missed until something forces a re-run, and it silently
# compiles anything that happens to sit under src/ - including the generated
# sources under src/matter/default_zap/zap-generated, which ncs_configure_data_model()
# already supplies to the matter-data-model target.
target_sources(app PRIVATE
    src/main.c
    src/air_quality_monitor.c
    src/components/event_handler.c
    src/components/matter_handler.cpp
    src/components/sensors.c
    src/drivers/led_controller.c
    src/utils/air_quality_mapper.c
    src/utils/air_quality_mapper.cpp
    src/utils/variable_buffer.c
    thirdparty/sensirion_gas_index_algorithm/sensirion_gas_index_algorithm.c
)


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
