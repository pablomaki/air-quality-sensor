#ifndef DEVICE_IDENTITY_H
#define DEVICE_IDENTITY_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Derive this board's Matter identity from its hardware ID and persist it
 *
 * Serial number, unique ID and setup discriminator are all compiled into the
 * image, so every board flashed with the same binary presents itself to a
 * controller as the same device. Each value is instead derived from the SoC's
 * factory programmed base MAC address, which makes one binary yield a distinct identity
 * per board and reproduces that same identity after a factory reset has wiped
 * storage - so a recommissioned board is still recognisably the same device.
 *
 * Writes the values straight into the Matter configuration in NVS, so it must
 * be called after nvs_flash_init() and before esp_matter::start(), which reads
 * them and begins advertising.
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int apply_device_identity(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_IDENTITY_H
