set(THIRDPARTY ${CMAKE_CURRENT_LIST_DIR}/../thirdparty)

set(srcs
    src/main.c
    src/board.c
    src/air_quality_monitor.c
    src/components/device_identity.cpp
    src/components/event_handler.c
    src/components/factory_reset.cpp
    src/components/matter_handler.cpp
    src/components/sensors.c
    src/drivers/led_controller.c
    src/drivers/sensirion_i2c.c
    src/utils/air_quality_mapper.cpp
    src/utils/variable_buffer.c
)
set(priv_includes)

if(CONFIG_ENABLE_SHT4X)
    list(APPEND srcs src/drivers/sht4x.c)
endif()

if(CONFIG_ENABLE_SGP40)
    list(APPEND srcs src/drivers/sgp40.c ${THIRDPARTY}/sensirion_gas_index_algorithm/sensirion_gas_index_algorithm.c)
    list(APPEND priv_includes ${THIRDPARTY}/sensirion_gas_index_algorithm)
endif()

if(CONFIG_ENABLE_SCD4X)
    list(APPEND srcs src/drivers/scd4x.c)
endif()

if(CONFIG_ENABLE_BMP390)
    list(APPEND srcs src/drivers/bmp390.c src/drivers/bosch_i2c.c ${THIRDPARTY}/bmp3/bmp3.c)
    list(APPEND priv_includes ${THIRDPARTY}/bmp3)
endif()

if(CONFIG_ENABLE_BME680)
    # BSEC is licensed by Bosch and not part of this repository. The C2/C3 build
    # is rv32imc with the soft-float ABI, which links into the C6 as is.
    if(DEFINED ENV{BSEC_DIR})
        set(BSEC_DIR $ENV{BSEC_DIR})
    else()
        set(BSEC_DIR $ENV{HOME}/Downloads/bsec2-6-1-0_generic_release_22102024)
    endif()
    set(BSEC_LIB ${BSEC_DIR}/algo/bsec_IAQ/bin/esp/esp32_c2c3/libalgobsec.a)
    set(BSEC_INC ${BSEC_DIR}/algo/bsec_IAQ/inc)
    if(NOT EXISTS ${BSEC_LIB})
        message(FATAL_ERROR
            "This variant needs the Bosch BSEC library, which was not found at\n"
            "  ${BSEC_LIB}\n"
            "Download BSEC 2.6.1 from Bosch Sensortec and point BSEC_DIR at the extracted release.")
    endif()

    list(APPEND srcs src/drivers/bme680_iaq.c src/drivers/bosch_i2c.c ${THIRDPARTY}/bme68x/bme68x.c)
    list(APPEND priv_includes ${THIRDPARTY}/bme68x ${BSEC_INC})
endif()

list(REMOVE_DUPLICATES srcs)

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS include .
                       PRIV_INCLUDE_DIRS ${priv_includes})

if(CONFIG_ENABLE_BME680)
    add_prebuilt_library(algobsec ${BSEC_LIB})
    target_link_libraries(${COMPONENT_LIB} PRIVATE algobsec)
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
