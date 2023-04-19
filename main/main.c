#include "btn.h"
#include "esp_log.h"
#include "findfile.h"
#include "flash.h"
#include "flash_args.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "led.h"
#include "usb.h"

static const char *TAG = "main";

static flash_args_t *flash_args = NULL;

static EventGroupHandle_t event_group;

#define FLASH_START_BIT BIT0

static void storage_mount_changed(bool mounted)
{
    if (mounted) {
        ESP_LOGI(TAG, "Storage mounted");
        led_set_status(LED_STATUS_READY);

        if (flash_args != NULL) {
            flash_args_free(flash_args);
        }
        const char *dir = CONFIG_TINYUSB_MSC_MOUNT_PATH;
        const char *fname = "flasher_args.json";
        findfile_t *ff = findfile(dir, fname);
        if (ff != NULL) {
            flash_args = flash_args_from_json(ff->buf, ff->size, ff->dir);
            if (flash_args != NULL) {
                flash_args_dump(flash_args);
            }
            free(ff->buf);
            if (ff->dir != dir) {
                free(ff->dir);
            }
            free(ff);
        }
        if (flash_args == NULL) {
            ESP_LOGE(TAG, "Cannot find \"%s\" file in the \"%s\" directory",
                     fname, dir);
            led_set_status(LED_STATUS_ERROR);
        }
    } else {
        ESP_LOGI(TAG, "Storage unmounted");
        led_set_status(LED_STATUS_USB);
    }
}

static void flash_check(void)
{
    if (usb_mounted()) {
        ESP_LOGW(TAG, "Storage exposed over USB, please remove it from PC");
    } else if (flash_args == NULL) {
        ESP_LOGW(TAG, "Not found flash args, please copy files to USB");
    } else if (xEventGroupGetBits(event_group) & FLASH_START_BIT) {
        ESP_LOGW(TAG, "Flashing, please wait");
    } else {
        led_set_status(LED_STATUS_FLASH);
        ESP_LOGI(TAG, "Flashing %d file(s)...", flash_args->flash_files_size);
        xEventGroupSetBits(event_group, FLASH_START_BIT);
    }
}

static void flash_done(bool success)
{
    if (success) {
        led_set_status(LED_STATUS_READY);
    } else {
        led_set_status(LED_STATUS_ERROR);
    }
    xEventGroupClearBits(event_group, FLASH_START_BIT);
}

void app_main(void)
{
    led_init();
    usb_init(storage_mount_changed);

    event_group = xEventGroupCreate();
    btn_init(flash_check, NULL, NULL);

    while (1) {
        xEventGroupWaitBits(event_group, FLASH_START_BIT, pdFALSE, pdFALSE,
                            portMAX_DELAY);
        flash(flash_args, flash_done);
    }
}
