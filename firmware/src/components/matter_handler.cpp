#include <components/matter_handler.h>
#include <utils/variable_buffer.h>
#include <components/event_handler.h>

#include "app/matter_init.h"
#include "app/task_executor.h"
#include "board/board.h"
#include "lib/core/CHIPError.h"
#include "lib/support/CodeUtils.h"

#include <setup_payload/OnboardingCodesUtil.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app/clusters/concentration-measurement-server/concentration-measurement-server.h>
#include <platform/CHIPDeviceLayer.h>
#include <utils/air_quality_mapper.h>

#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(matter_handler);

constexpr uint8_t kTemperatureSensorEndpointId = 0x01;
constexpr uint8_t kHumiditySensorEndpointId = 0x02;
constexpr uint8_t kPressureSensorEndpointId = 0x03;
constexpr uint8_t kAirQualitySensorEndpointId = 0x04;

#if defined(CONFIG_ENABLE_SGP40) || defined(CONFIG_ENABLE_BME680)
// AirQuality is an enum attribute, not a scaled measurement, so it can only be
// updated through the cluster's delegate Instance, never a raw Attributes::Set().
using chip::app::Clusters::AirQuality::Feature;
static chip::app::Clusters::AirQuality::Instance sAirQualityInstance(
    kAirQualitySensorEndpointId,
    chip::BitMask<Feature>(Feature::kFair, Feature::kModerate, Feature::kVeryPoor, Feature::kExtremelyPoor));
#endif

#ifdef CONFIG_ENABLE_SCD4X
// Concentration measurement clusters also use a delegate Instance, never a raw Attributes::Set().
static chip::app::Clusters::ConcentrationMeasurement::Instance<true, false, false, false, false, false> sCarbonDioxideInstance(
    kAirQualitySensorEndpointId, chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Id,
    chip::app::Clusters::ConcentrationMeasurement::MeasurementMediumEnum::kAir,
    chip::app::Clusters::ConcentrationMeasurement::MeasurementUnitEnum::kPpm);
#endif

void handle_state_update()
{
    switch (Nrf::GetBoard().GetDeviceState())
    {
    case Nrf::DeviceState::DeviceAdvertisingBLE:
        dispatch_event(ADVERTISING_BLE);
        break;
    case Nrf::DeviceState::DeviceDisconnected:
        break;
    case Nrf::DeviceState::DeviceConnectedBLE:
        dispatch_event(BLE_CONNECTION_SUCCESS);
        break;
    case Nrf::DeviceState::DeviceProvisioned:
        dispatch_event(PROVISIONING_SUCCESS);
        break;
    default:
        break;
    }
}

int init_matter(void)
{
    int rc = 0;
    CHIP_ERROR err;
    err = Nrf::Matter::PrepareServer();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to prepare Matter server: %d", err.AsInteger());
        return err.AsInteger();
    }

    if (!Nrf::GetBoard().Init())
    {
        LOG_ERR("User interface initialization failed.");
        return CHIP_ERROR_INCORRECT_STATE.AsInteger();
    }

    err = Nrf::Matter::RegisterEventHandler(Nrf::Board::DefaultMatterEventHandler, 0);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to register Matter event handler: %d", err.AsInteger());
        return err.AsInteger();
    }

    err = Nrf::Matter::StartServer();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to start Matter server: %d", err.AsInteger());
        return err.AsInteger();
    }

#if defined(CONFIG_ENABLE_SGP40) || defined(CONFIG_ENABLE_BME680)
    if (sAirQualityInstance.Init() != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to initialize AirQuality cluster instance");
        return CHIP_ERROR_INCORRECT_STATE.AsInteger();
    }
#endif
#ifdef CONFIG_ENABLE_SCD4X
    if (sCarbonDioxideInstance.Init() != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to initialize CarbonDioxideConcentrationMeasurement cluster instance");
        return CHIP_ERROR_INCORRECT_STATE.AsInteger();
    }
#endif
    return rc;
}

int update_cluster_states(void)
{
    // This runs on the periodic-task workqueue thread, not on the CHIP event loop.
    // Every data-model access below (attribute Set() and the cluster Instance
    // setters) touches the attribute store and the reporting engine, which the CHIP
    // thread uses concurrently, so the stack lock has to be held for all of it.
    // RAII: released on every return path.
    chip::DeviceLayer::StackLock stackLock;

    LOG_INF("Updating advertisement data.");
    int rc = 0;

    // Each block below is its own scope so that `status` is declared by whichever
    // blocks are actually compiled in. It used to be declared in the
    // temperature/humidity block and used from the pressure and air quality
    // blocks, so a pressure-only or BME680-only build did not compile.

    // Every measured-value attribute used here is nullable, and null is the only
    // honest way to say "no reading". Publishing a placeholder number instead
    // makes an unavailable sensor indistinguishable from a working one.

#if defined(CONFIG_ENABLE_SHT4X) || defined(CONFIG_ENABLE_SCD4X)
    {
        float temperature = 0.0f;
        chip::Protocols::InteractionModel::Status status =
            get_mean(TEMPERATURE, &temperature)
                ? chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
                      kTemperatureSensorEndpointId, static_cast<int16_t>(temperature * 100))
                : chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::SetNull(
                      kTemperatureSensorEndpointId);
        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            LOG_ERR("Failed to update TemperatureMeasurement MeasuredValue: %d", static_cast<int>(status));
            rc |= 1 << 0;
        }
    }
    {
        float humidity = 0.0f;
        chip::Protocols::InteractionModel::Status status =
            get_mean(HUMIDITY, &humidity)
                ? chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Set(
                      kHumiditySensorEndpointId, static_cast<uint16_t>(humidity * 100))
                : chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::SetNull(
                      kHumiditySensorEndpointId);
        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            LOG_ERR("Failed to update RelativeHumidityMeasurement MeasuredValue: %d", static_cast<int>(status));
            rc |= 1 << 1;
        }
    }
#endif

#if defined(CONFIG_ENABLE_BMP390) || defined(CONFIG_ENABLE_BME680)
    {
        float pressure = 0.0f;
        chip::Protocols::InteractionModel::Status status =
            get_mean(PRESSURE, &pressure)
                ? chip::app::Clusters::PressureMeasurement::Attributes::MeasuredValue::Set(
                      kPressureSensorEndpointId, static_cast<int16_t>(pressure * 100))
                : chip::app::Clusters::PressureMeasurement::Attributes::MeasuredValue::SetNull(
                      kPressureSensorEndpointId);
        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            LOG_ERR("Failed to update PressureMeasurement MeasuredValue: %d", static_cast<int>(status));
            rc |= 1 << 2;
        }
    }
#endif

#ifdef CONFIG_ENABLE_SCD4X
    {
        float co2_concentration = 0.0f;
        CHIP_ERROR err = get_mean(CO2_CONCENTRATION, &co2_concentration)
                             ? sCarbonDioxideInstance.SetMeasuredValue(
                                   chip::app::DataModel::MakeNullable(co2_concentration))
                             : sCarbonDioxideInstance.SetMeasuredValue(chip::app::DataModel::NullNullable);
        if (err != CHIP_NO_ERROR)
        {
            LOG_ERR("Failed to update CarbonDioxideConcentrationMeasurement MeasuredValue: %" CHIP_ERROR_FORMAT, err.Format());
            rc |= 1 << 3;
        }
    }
#endif

    // Note: SGP40 and BME680 both drive the single AirQuality attribute, so in a
    // build with both enabled the later one wins. Deciding which source takes
    // precedence is left for the air quality index rework.
#ifdef CONFIG_ENABLE_SGP40
    {
        // AirQuality is an enum with no null state; kUnknown is how it says
        // "no reading", and air_quality_enum_from_index() maps out-of-range
        // input to exactly that.
        float voc_index = 0.0f;
        chip::Protocols::InteractionModel::Status status =
            sAirQualityInstance.UpdateAirQuality(
                get_mean(VOC_INDEX, &voc_index)
                    ? air_quality_enum_from_index(static_cast<int>(voc_index))
                    : chip::app::Clusters::AirQuality::AirQualityEnum::kUnknown);
        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            LOG_ERR("Failed to update AirQuality attribute: %d", static_cast<int>(status));
            rc |= 1 << 4;
        }
    }
#endif
#ifdef CONFIG_ENABLE_BME680
    {
        float iaq_index = 0.0f;
        chip::Protocols::InteractionModel::Status status =
            sAirQualityInstance.UpdateAirQuality(
                get_mean(IAQ_INDEX, &iaq_index)
                    ? air_quality_enum_from_index(static_cast<int>(iaq_index))
                    : chip::app::Clusters::AirQuality::AirQualityEnum::kUnknown);
        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            LOG_ERR("Failed to update AirQuality attribute: %d", static_cast<int>(status));
            rc |= 1 << 5;
        }
    }
#endif
    return rc;
}

void matter_dispatch_tasks(void)
{
    while (true)
    {
        Nrf::DispatchNextTask();
    }
}