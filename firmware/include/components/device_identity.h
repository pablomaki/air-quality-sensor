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
 * factory programmed device ID, which makes one binary yield a distinct identity
 * per board and reproduces that same identity after a factory reset has wiped
 * storage - so a recommissioned board is still recognisably the same device.
 *
 * Must be called after Nrf::Matter::PrepareServer(), which brings up the
 * configuration manager, and before Nrf::Matter::StartServer(), which begins
 * advertising with these values.
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int apply_device_identity(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_IDENTITY_H
