#ifndef THREAD_MONITOR_H
#define THREAD_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start watching the Thread link: periodic radio diagnostics and the detached reboot
 *
 * Logs the device role, the link to the parent and the radio counters of the
 * past interval, and reboots a commissioned device that has stayed detached for
 * CONFIG_THREAD_DETACHED_REBOOT_MINUTES. Must be called after esp_matter::start(),
 * which creates the OpenThread instance.
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_thread_monitor(void);

#ifdef __cplusplus
}
#endif

#endif // THREAD_MONITOR_H
