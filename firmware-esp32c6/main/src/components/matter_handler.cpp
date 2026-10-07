#include <components/matter_handler.h>
#include <components/device_identity.h>
#include <utils/variable_buffer.h>
#include <utils/air_quality_mapper.h>

#include <esp_log.h>
#include <esp_matter.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <app/clusters/air-quality-server/AirQualityCluster.h>
#include <app/server/Server.h>
#include <data_model_provider/clusters/pressure_measurement/integration.h>
#include <data_model_provider/clusters/relative_humidity_measurement/integration.h>
#include <data_model_provider/clusters/temperature_measurement/integration.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/ESP32/OpenthreadLauncher.h>

static const char *TAG = "matter_handler";

using namespace esp_matter;
using namespace chip::app::Clusters;

#if defined(CONFIG_ENABLE_SHT4X) || defined(CONFIG_ENABLE_SCD4X) || defined(CONFIG_ENABLE_BME680)
#define MEASURES_TEMPERATURE_AND_HUMIDITY 1
#endif

#if defined(CONFIG_ENABLE_BMP390) || defined(CONFIG_ENABLE_BME680)
#define MEASURES_PRESSURE 1
#endif

namespace
{

/**
 * @brief How long a freshly updated image may take to reach the network
 *
 * Until it does, the image is unconfirmed and the bootloader falls back to the
 * previous one on the next reset. Thread attach and SRP registration take
 * seconds, so this only expires when the new image cannot get back online.
 */
constexpr uint64_t kImageConfirmTimeoutUs = 10ULL * 60 * 1000 * 1000;

[[maybe_unused]] uint16_t sTemperatureEndpointId;
[[maybe_unused]] uint16_t sHumidityEndpointId;
[[maybe_unused]] uint16_t sPressureEndpointId;
uint16_t sAirQualityEndpointId;

esp_timer_handle_t sImageConfirmTimer;
bool sImagePendingVerify;

/**
 * @brief Roll back to the previous image, the new one never came online
 *
 * @param arg Unused
 */
void RollBackImage(void *arg)
{
    (void)arg;
    ESP_LOGE(TAG, "Updated image did not reach the network in time, rolling back.");
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

/**
 * @brief Keep the running image, it has proven that it can come online
 */
void ConfirmImage()
{
    if (!sImagePendingVerify)
    {
        return;
    }

    sImagePendingVerify = false;
    esp_timer_stop(sImageConfirmTimer);

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to confirm the updated image (err %d).", err);
        return;
    }
    ESP_LOGI(TAG, "Updated image confirmed.");
}

/**
 * @brief Arm the rollback timer when this is the first boot of an updated image
 *
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t ArmImageConfirmation()
{
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY)
    {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "First boot of an updated image, confirming once Matter is reachable.");
    sImagePendingVerify = true;

    const esp_timer_create_args_t timerArgs = {
        .callback = RollBackImage,
        .name = "image_confirm",
    };
    esp_err_t err = esp_timer_create(&timerArgs, &sImageConfirmTimer);
    if (err != ESP_OK)
    {
        return err;
    }
    return esp_timer_start_once(sImageConfirmTimer, kImageConfirmTimeoutUs);
}

/**
 * @brief Handle Matter stack events
 *
 * DNS-SD comes up once the device has attached to Thread and registered with
 * the border router's SRP server, which is what an OTA provider or controller
 * needs to reach it, so that is the point an updated image counts as working.
 *
 * @param event Device event
 * @param arg Unused
 */
void MatterEventHandler(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg)
{
    (void)arg;

    switch (event->Type)
    {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete.");
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGW(TAG, "Commissioning failed, fail safe timer expired.");
        break;
    case chip::DeviceLayer::DeviceEventType::kDnssdInitialized:
        ConfirmImage();
        break;
    default:
        break;
    }
}

/**
 * @brief Matter attribute callback; nothing here is writable, so every update is accepted
 */
esp_err_t AttributeUpdateCallback(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                  uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    return ESP_OK;
}

/**
 * @brief Matter identify callback; the LED is reserved for status, so identify is a no-op
 */
esp_err_t IdentificationCallback(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                 uint8_t effect_variant, void *priv_data)
{
    return ESP_OK;
}

/**
 * @brief Create one endpoint per measured quantity, in the order of the nRF data model
 *
 * @param node Root node
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t CreateEndpoints(node_t *node)
{
#ifdef MEASURES_TEMPERATURE_AND_HUMIDITY
    endpoint::temperature_sensor::config_t temperatureConfig;
    endpoint_t *temperature = endpoint::temperature_sensor::create(node, &temperatureConfig, ENDPOINT_FLAG_NONE, nullptr);
    VerifyOrReturnError(temperature != nullptr, ESP_FAIL);
    sTemperatureEndpointId = endpoint::get_id(temperature);

    endpoint::humidity_sensor::config_t humidityConfig;
    endpoint_t *humidity = endpoint::humidity_sensor::create(node, &humidityConfig, ENDPOINT_FLAG_NONE, nullptr);
    VerifyOrReturnError(humidity != nullptr, ESP_FAIL);
    sHumidityEndpointId = endpoint::get_id(humidity);
#endif

#ifdef MEASURES_PRESSURE
    endpoint::pressure_sensor::config_t pressureConfig;
    endpoint_t *pressure = endpoint::pressure_sensor::create(node, &pressureConfig, ENDPOINT_FLAG_NONE, nullptr);
    VerifyOrReturnError(pressure != nullptr, ESP_FAIL);
    sPressureEndpointId = endpoint::get_id(pressure);
#endif

    // Always present: Air Quality Sensor is the device type of the product, and
    // AirQuality reports kUnknown when no gas sensor is fitted.
    endpoint::air_quality_sensor::config_t airQualityConfig;
    endpoint_t *airQuality = endpoint::air_quality_sensor::create(node, &airQualityConfig, ENDPOINT_FLAG_NONE, nullptr);
    VerifyOrReturnError(airQuality != nullptr, ESP_FAIL);
    sAirQualityEndpointId = endpoint::get_id(airQuality);

    cluster_t *airQualityCluster = cluster::get(airQuality, AirQuality::Id);
    VerifyOrReturnError(airQualityCluster != nullptr, ESP_FAIL);
    cluster::air_quality::feature::fair::add(airQualityCluster);
    cluster::air_quality::feature::moderate::add(airQualityCluster);
    cluster::air_quality::feature::very_poor::add(airQualityCluster);
    cluster::air_quality::feature::extremely_poor::add(airQualityCluster);

#ifdef CONFIG_ENABLE_SCD4X
    cluster::carbon_dioxide_concentration_measurement::config_t co2Config;
    co2Config.measurement_medium = chip::to_underlying(CarbonDioxideConcentrationMeasurement::MeasurementMediumEnum::kAir);
    co2Config.feature_flags = cluster::concentration_measurement::feature::numeric_measurement::get_id();
    co2Config.features.numeric_measurement.measurement_unit =
        chip::to_underlying(CarbonDioxideConcentrationMeasurement::MeasurementUnitEnum::kPpm);
    VerifyOrReturnError(
        cluster::carbon_dioxide_concentration_measurement::create(airQuality, &co2Config, CLUSTER_FLAG_SERVER) != nullptr,
        ESP_FAIL);
#endif

    return ESP_OK;
}

/**
 * @brief Report the air quality rating through the code driven AirQuality cluster
 *
 * Its attribute is not held in the esp-matter attribute store, so it is set on
 * the cluster object the data model provider registered for the endpoint.
 *
 * @param airQuality Rating to report
 * @return chip::Protocols::InteractionModel::Status, Success if ok
 */
chip::Protocols::InteractionModel::Status SetAirQuality(AirQuality::AirQualityEnum airQuality)
{
    chip::app::ServerClusterInterface *cluster = data_model::provider::get_instance().registry().Get(
        chip::app::ConcreteClusterPath(sAirQualityEndpointId, AirQuality::Id));
    if (cluster == nullptr)
    {
        return chip::Protocols::InteractionModel::Status::UnsupportedCluster;
    }
    return static_cast<AirQualityCluster *>(cluster)->SetAirQuality(airQuality);
}

} // namespace

int init_matter(void)
{
    node::config_t nodeConfig;
    node_t *node = node::create(&nodeConfig, AttributeUpdateCallback, IdentificationCallback);
    if (node == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create the Matter node.");
        return ESP_FAIL;
    }

    esp_err_t err = CreateEndpoints(node);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create the Matter endpoints (err %d).", err);
        return err;
    }

    int rc = apply_device_identity();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to apply the device identity (err %d).", rc);
        return rc;
    }

    err = ArmImageConfirmation();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to arm the image confirmation (err %d).", err);
    }

    esp_openthread_platform_config_t threadConfig = {
        .radio_config = {.radio_mode = RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = HOST_CONNECTION_MODE_NONE},
        .port_config = {.storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10},
    };
    set_openthread_platform_config(&threadConfig);

    err = esp_matter::start(MatterEventHandler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Matter (err %d).", err);
        return err;
    }

    // An uncommissioned device has no network to come back to, so there is
    // nothing more to prove before keeping the image.
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
    {
        ConfirmImage();
    }
    return 0;
}

int update_cluster_states(void)
{
    chip::DeviceLayer::StackLock stackLock;

    ESP_LOGI(TAG, "Updating advertisement data.");
    int rc = 0;
    [[maybe_unused]] CHIP_ERROR err;

#ifdef MEASURES_TEMPERATURE_AND_HUMIDITY
    float temperature = 0.0f;
    err = TemperatureMeasurement::SetMeasuredValue(
        sTemperatureEndpointId, get_mean(TEMPERATURE, &temperature)
                                    ? chip::app::DataModel::MakeNullable(static_cast<int16_t>(temperature * 100))
                                    : chip::app::DataModel::Nullable<int16_t>());
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to update TemperatureMeasurement MeasuredValue: %" CHIP_ERROR_FORMAT, err.Format());
        rc |= 1 << 0;
    }

    float humidity = 0.0f;
    err = RelativeHumidityMeasurement::SetMeasuredValue(
        sHumidityEndpointId, get_mean(HUMIDITY, &humidity)
                                 ? chip::app::DataModel::MakeNullable(static_cast<uint16_t>(humidity * 100))
                                 : chip::app::DataModel::Nullable<uint16_t>());
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to update RelativeHumidityMeasurement MeasuredValue: %" CHIP_ERROR_FORMAT, err.Format());
        rc |= 1 << 1;
    }
#endif

#ifdef MEASURES_PRESSURE
    // MeasuredValue is in units of 0.1 kPa, which is exactly 1 hPa
    float pressure_hpa = 0.0f;
    err = PressureMeasurement::SetMeasuredValue(
        sPressureEndpointId, get_mean(PRESSURE, &pressure_hpa)
                                 ? chip::app::DataModel::MakeNullable(static_cast<int16_t>(pressure_hpa + 0.5f))
                                 : chip::app::DataModel::Nullable<int16_t>());
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to update PressureMeasurement MeasuredValue: %" CHIP_ERROR_FORMAT, err.Format());
        rc |= 1 << 2;
    }
#endif

#ifdef CONFIG_ENABLE_SCD4X
    float co2_concentration = 0.0f;
    esp_matter_attr_val_t co2Value = esp_matter_nullable_float(
        get_mean(CO2_CONCENTRATION, &co2_concentration) ? nullable<float>(co2_concentration) : nullable<float>());
    if (attribute::update(sAirQualityEndpointId, CarbonDioxideConcentrationMeasurement::Id,
                          CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id, &co2Value) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to update CarbonDioxideConcentrationMeasurement MeasuredValue.");
        rc |= 1 << 3;
    }
#endif

    // One writer for the air quality rating, from the best gas sensor fitted,
    // preferring the BME680 IAQ index over the SGP40 VOC index. With neither the
    // attribute stays kUnknown, which is a valid value and keeps the mandatory
    // cluster of this endpoint's device type answering.
    {
        using chip::app::Clusters::AirQuality::AirQualityEnum;
        AirQualityEnum air_quality = AirQualityEnum::kUnknown;

#if defined(CONFIG_ENABLE_BME680)
        float iaq_index = 0.0f;
        if (get_mean(IAQ_INDEX, &iaq_index))
        {
            air_quality = air_quality_enum_from_index(static_cast<int>(iaq_index));
        }
#elif defined(CONFIG_ENABLE_SGP40)
        float voc_index = 0.0f;
        if (get_mean(VOC_INDEX, &voc_index))
        {
            air_quality = air_quality_enum_from_index(static_cast<int>(voc_index));
        }
#endif

        chip::Protocols::InteractionModel::Status status = SetAirQuality(air_quality);
        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            ESP_LOGE(TAG, "Failed to update AirQuality attribute: %d", static_cast<int>(status));
            rc |= 1 << 4;
        }
    }
    return rc;
}
