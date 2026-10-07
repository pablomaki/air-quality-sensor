#include <board.h>

esp_err_t board_init_rf_switch(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(BOARD_RF_SWITCH_ENABLE_GPIO) | BIT64(BOARD_RF_ANTENNA_SELECT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK)
    {
        return err;
    }

    err = gpio_set_level(BOARD_RF_SWITCH_ENABLE_GPIO, 0);
    if (err != ESP_OK)
    {
        return err;
    }
    return gpio_set_level(BOARD_RF_ANTENNA_SELECT_GPIO, 0);
}
