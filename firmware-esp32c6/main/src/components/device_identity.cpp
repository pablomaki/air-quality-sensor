#include <components/device_identity.h>

#include <crypto/CHIPCryptoPAL.h>
#include <lib/support/BytesToHex.h>
#include <platform/ConfigurationManager.h>
#include <platform/ESP32/ESP32Config.h>

#include <esp_log.h>
#include <esp_mac.h>

#include <cstring>

static const char *TAG = "device_identity";

using chip::DeviceLayer::ConfigurationManager;
using chip::DeviceLayer::Internal::ESP32Config;

namespace
{

constexpr char kSerialNumberPrefix[] = "AQS-";
constexpr size_t kSerialNumberPrefixLength = sizeof(kSerialNumberPrefix) - 1;

// Domain separator for the unique ID. The specification asks for a unique ID
// that is not simply the serial number restated, so the hardware ID is hashed
// rather than used directly.
constexpr char kUniqueIdSalt[] = "air-quality-sensor/unique-id/v1";
constexpr size_t kUniqueIdSaltLength = sizeof(kUniqueIdSalt) - 1;

// The unique ID attribute holds at most 32 characters, so 16 bytes of hash.
constexpr size_t kUniqueIdBytes = 16;

// The setup discriminator is a 12 bit value.
constexpr uint16_t kDiscriminatorMask = 0x0FFF;

constexpr size_t kHardwareIdBytes = 6;

struct DerivedIdentity
{
    char serialNumber[ConfigurationManager::kMaxSerialNumberLength + 1];
    char uniqueId[ConfigurationManager::kMaxUniqueIDLength + 1];
    uint16_t discriminator;
};

/**
 * @brief Derive the identity values from the SoC's factory programmed base MAC address
 *
 * @param identity Destination for the derived values
 * @return CHIP_ERROR, CHIP_NO_ERROR if ok
 */
CHIP_ERROR DeriveIdentity(DerivedIdentity &identity)
{
    uint8_t hardwareId[kHardwareIdBytes];
    VerifyOrReturnError(esp_efuse_mac_get_default(hardwareId) == ESP_OK, CHIP_ERROR_INCORRECT_STATE);

    memcpy(identity.serialNumber, kSerialNumberPrefix, kSerialNumberPrefixLength);
    ReturnErrorOnFailure(chip::Encoding::BytesToUppercaseHexString(hardwareId, sizeof(hardwareId),
                                                                  identity.serialNumber + kSerialNumberPrefixLength,
                                                                  sizeof(identity.serialNumber) - kSerialNumberPrefixLength));

    uint8_t salted[kUniqueIdSaltLength + kHardwareIdBytes];
    memcpy(salted, kUniqueIdSalt, kUniqueIdSaltLength);
    memcpy(salted + kUniqueIdSaltLength, hardwareId, sizeof(hardwareId));

    uint8_t hash[chip::Crypto::kSHA256_Hash_Length];
    ReturnErrorOnFailure(chip::Crypto::Hash_SHA256(salted, sizeof(salted), hash));

    ReturnErrorOnFailure(
        chip::Encoding::BytesToUppercaseHexString(hash, kUniqueIdBytes, identity.uniqueId, sizeof(identity.uniqueId)));

    // Bytes the unique ID did not consume, so the discriminator is not a prefix
    // of a value the device already advertises.
    identity.discriminator =
        static_cast<uint16_t>(((hash[kUniqueIdBytes] << 8) | hash[kUniqueIdBytes + 1]) & kDiscriminatorMask);

    return CHIP_NO_ERROR;
}

/**
 * @brief Persist a string configuration value, skipping the write when it already matches
 *
 * @param key Configuration key
 * @param value Value to store
 * @return CHIP_ERROR, CHIP_NO_ERROR if ok
 */
CHIP_ERROR ApplyString(ESP32Config::Key key, const char *value)
{
    char current[ConfigurationManager::kMaxSerialNumberLength + ConfigurationManager::kMaxUniqueIDLength + 1] = {};
    size_t length = 0;

    if (ESP32Config::ReadConfigValueStr(key, current, sizeof(current), length) == CHIP_NO_ERROR &&
        strcmp(current, value) == 0)
    {
        return CHIP_NO_ERROR;
    }
    return ESP32Config::WriteConfigValueStr(key, value);
}

/**
 * @brief Persist the setup discriminator, skipping the write when it already matches
 *
 * @param discriminator Discriminator to store
 * @return CHIP_ERROR, CHIP_NO_ERROR if ok
 */
CHIP_ERROR ApplyDiscriminator(uint16_t discriminator)
{
    uint32_t current = 0;

    if (ESP32Config::ReadConfigValue(ESP32Config::kConfigKey_SetupDiscriminator, current) == CHIP_NO_ERROR &&
        current == discriminator)
    {
        return CHIP_NO_ERROR;
    }
    return ESP32Config::WriteConfigValue(ESP32Config::kConfigKey_SetupDiscriminator, static_cast<uint32_t>(discriminator));
}

} // namespace

int apply_device_identity(void)
{
    DerivedIdentity identity = {};

    CHIP_ERROR err = DeriveIdentity(identity);
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to derive the device identity (err %" CHIP_ERROR_FORMAT ").", err.Format());
        return static_cast<int>(err.AsInteger());
    }

    err = ApplyString(ESP32Config::kConfigKey_SerialNum, identity.serialNumber);
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to store the serial number (err %" CHIP_ERROR_FORMAT ").", err.Format());
        return static_cast<int>(err.AsInteger());
    }

    err = ApplyString(ESP32Config::kConfigKey_UniqueId, identity.uniqueId);
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to store the unique ID (err %" CHIP_ERROR_FORMAT ").", err.Format());
        return static_cast<int>(err.AsInteger());
    }

    err = ApplyDiscriminator(identity.discriminator);
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to store the setup discriminator (err %" CHIP_ERROR_FORMAT ").", err.Format());
        return static_cast<int>(err.AsInteger());
    }

    ESP_LOGI(TAG, "Device identity: serial number %s, discriminator 0x%03X.", identity.serialNumber,
             identity.discriminator);
    return 0;
}
