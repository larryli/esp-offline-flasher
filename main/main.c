#include "btn.h"
#include "esp_log.h"
#include "flash_args.h"
#include "led.h"
#include "usb.h"

static const char *TAG = "main";

static flash_args_t *flash_args = NULL;

static void storage_mount_changed(bool mounted)
{
    if (mounted) {
        ESP_LOGI(TAG, "Storage mounted");
        led_set_status(LED_STATUS_READY);

        if (flash_args != NULL) {
            flash_args_free(flash_args);
        }
        flash_args = flash_args_find(CONFIG_TINYUSB_MSC_MOUNT_PATH);
        if (flash_args != NULL) {
            flash_args_dump(flash_args);
        } else {
            led_set_status(LED_STATUS_ERROR);
        }
    } else {
        ESP_LOGI(TAG, "Storage unmounted");
        led_set_status(LED_STATUS_USB);
    }
}

static void flash(void)
{
    if (usb_mounted()) {
        ESP_LOGW(TAG, "Storage exposed over USB, please remove it from PC.");
    } else {
        led_set_status(LED_STATUS_FLASH);
        ESP_LOGI(TAG, "Flashing...");
        flash_args_dump(flash_args);
    }
}

void app_main(void)
{
    led_init();
    btn_init(flash, NULL, NULL);
    usb_init(storage_mount_changed);
}
