#ifndef BLUETOOTH_HANDLER_H
#define BLUETOOTH_HANDLER_H

#include <zephyr/bluetooth/bluetooth.h>

int init_ble(void);

void per_adv_sync_cb(struct bt_le_per_adv_sync *sync, struct bt_le_per_adv_sync_synced_info *info);

void per_adv_sync_lost_cb(struct bt_le_per_adv_sync *sync, const struct bt_le_per_adv_sync_term_info *info);

int restart_edvertising(void);

int start_advertising(void);

int update_advertising_data(void);

#endif // BLUETOOTH_HANDLER_H