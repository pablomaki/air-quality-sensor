#include <components/device_identity.h>

#include <crypto/CHIPCryptoPAL.h>
#include <lib/support/BytesToHex.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/ConfigurationManager.h>
#include <platform/DeviceInstanceInfoProvider.h>

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>

#include <cstring>

LOG_MODULE_REGISTER(device_identity);

using chip::DeviceLayer::ConfigurationManager;
using chip::DeviceLayer::ConfigurationMgr;
using chip::DeviceLayer::GetCommissionableDataProvider;
using chip::DeviceLayer::GetDeviceInstanceInfoProvider;

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

// hwinfo reports 8 bytes on this SoC. The cap keeps the hex encoded serial
// number inside kMaxSerialNumberLength even if a future part reports more.
constexpr size_t kHardwareIdMaxBytes = (ConfigurationManager::kMaxSerialNumberLength - kSerialNumberPrefixLength) / 2;

struct DerivedIdentity
{
    char serialNumber[ConfigurationManager::kMaxSerialNumberLength + 1];
    char uniqueId[ConfigurationManager::kMaxUniqueIDLength + 1];
    uint16_t discriminator;
};

/**
 * @brief Derive the identity values from the SoC's factory programmed device ID
 *
 * @param identity Destination for the derived values
 * @return CHIP_ERROR, CHIP_NO_ERROR if ok
 */
CHIP_ERROR DeriveIdentity(DerivedIdentity &identity)
{
    uint8_t hardwareId[kHardwareIdMaxBytes];
    ssize_t hardwareIdSize = hwinfo_get_device_id(hardwareId, sizeof(hardwareId));
    VerifyOrReturnError(hardwareIdSize > 0, CHIP_ERROR_INCORRECT_STATE);

    memcpy(identity.serialNumber, kSerialNumberPrefix, kSerialNumberPrefixLength);
    ReturnErrorOnFailure(chip::Encoding::BytesToUppercaseHexString(hardwareId, static_cast<size_t>(hardwareIdSize),
                                                                  identity.serialNumber + kSerialNumberPrefixLength,
                                                                  sizeof(identity.serialNumber) - kSerialNumberPrefixLength));

    uint8_t salted[kUniqueIdSaltLength + kHardwareIdMaxBytes];
    memcpy(salted, kUniqueIdSalt, kUniqueIdSaltLength);
    memcpy(salted + kUniqueIdSaltLength, hardwareId, static_cast<size_t>(hardwareIdSize));

    uint8_t hash[chip::Crypto::kSHA256_Hash_Length];
    ReturnErrorOnFailure(chip::Crypto::Hash_SHA256(salted, kUniqueIdSaltLength + static_cast<size_t>(hardwareIdSize), hash));

    ReturnErrorOnFailure(
        chip::Encoding::BytesToUppercaseHexString(hash, kUniqueIdBytes, identity.uniqueId, sizeof(identity.uniqueId)));

    // Bytes the unique ID did not consume, so the discriminator is not a prefix
    // of a value the device already advertises.
    identity.discriminator =
        static_cast<uint16_t>(((hash[kUniqueIdBytes] << 8) | hash[kUniqueIdBytes + 1]) & kDiscriminatorMask);

    return CHIP_NO_ERROR;
}

/**
 * @brief Persist the serial number, skipping the write when it already matches
 *
 * @param serialNumber Serial number to store
 * @return CHIP_ERROR, CHIP_NO_ERROR if ok
 */
CHIP_ERROR ApplySerialNumber(const char *serialNumber)
{
    char current[ConfigurationManager::kMaxSerialNumberLength + 1] = {};

    if (GetDeviceInstanceInfoProvider()->GetSerialNumber(current, sizeof(current)) == CHIP_NO_ERROR &&
        strcmp(current, serialNumber) == 0)
    {
        return CHIP_NO_ERROR;
    }
    return ConfigurationMgr().StoreSerialNumber(serialNumber, strlen(serialNumber));
}

/**
 * @brief Persist the unique ID, skipping the write when it already matches
 *
 * @param uniqueId Unique ID to store
 * @return CHIP_ERROR, CHIP_NO_ERROR if ok
 */
CHIP_ERROR ApplyUniqueId(const char *uniqueId)
{
    char current[ConfigurationManager::kMaxUniqueIDLength + 1] = {};

    if (ConfigurationMgr().GetUniqueId(current, sizeof(current)) == CHIP_NO_ERROR && strcmp(current, uniqueId) == 0)
    {
        return CHIP_NO_ERROR;
    }
    return ConfigurationMgr().StoreUniqueId(uniqueId, strlen(uniqueId));
}

/**
 * @brief Persist the setup discriminator, skipping the write when it already matches
 *
 * @param discriminator Discriminator to store
 * @return CHIP_ERROR, CHIP_NO_ERROR if ok
 */
CHIP_ERROR ApplyDiscriminator(uint16_t discriminator)
{
    uint16_t current = 0;

    if (GetCommissionableDataProvider()->GetSetupDiscriminator(current) == CHIP_NO_ERROR && current == discriminator)
    {
        return CHIP_NO_ERROR;
    }
    return GetCommissionableDataProvider()->SetSetupDiscriminator(discriminator);
}

} // namespace

int apply_device_identity(void)
{
    DerivedIdentity identity = {};

    CHIP_ERROR err = DeriveIdentity(identity);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to derive the device identity (err %d).", err.AsInteger());
        return err.AsInteger();
    }

    err = ApplySerialNumber(identity.serialNumber);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to store the serial number (err %d).", err.AsInteger());
        return err.AsInteger();
    }

    err = ApplyUniqueId(identity.uniqueId);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to store the unique ID (err %d).", err.AsInteger());
        return err.AsInteger();
    }

    err = ApplyDiscriminator(identity.discriminator);
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Failed to store the setup discriminator (err %d).", err.AsInteger());
        return err.AsInteger();
    }

    LOG_INF("Device identity: serial number %s, discriminator 0x%03X.", identity.serialNumber, identity.discriminator);
    return 0;
}
