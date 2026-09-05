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
#include <platform/CHIPDeviceLayer.h>

#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(matter_handler);

constexpr uint8_t kTemperatureSensorEndpointId = 0x01;
constexpr uint8_t kHumiditySensorEndpointId = 0x02;

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
    return rc;
}

int update_cluster_states(void)
{
    LOG_INF("Updating advertisement data.");

    int rc = 0;
    static float temperature = 1.0; // get_mean(TEMPERATURE);
    temperature = temperature + 1.0;
    chip::Protocols::InteractionModel::Status status =
        chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
            kTemperatureSensorEndpointId, static_cast<int16_t>(temperature * 100));
    if (status != chip::Protocols::InteractionModel::Status::Success)
    {
        LOG_ERR("Failed to update TemperatureMeasurement MeasuredValue: %d", static_cast<int>(status));
    }
    static float humidity = 1.0; // get_mean(HUMIDITY);
    humidity = humidity + 1.0;
    status = chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Set(
        kHumiditySensorEndpointId, static_cast<int16_t>(humidity * 100));
    if (status != chip::Protocols::InteractionModel::Status::Success)
    {
        LOG_ERR("Failed to update RelativeHumidityMeasurement MeasuredValue: %d", static_cast<int>(status));
    }

#if defined(CONFIG_ENABLE_BMP390) || defined(CONFIG_ENABLE_BME680)
    float pressure = get_mean(PRESSURE);

#endif

#ifdef CONFIG_ENABLE_SCD4X
    float co2_concentration = get_mean(CO2_CONCENTRATION);
#endif

#ifdef CONFIG_ENABLE_SGP40
    float voc_index = get_mean(VOC_INDEX);
#endif
#ifdef CONFIG_ENABLE_BME680
    float iaq_index = get_mean(IAQ_INDEX);
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