#include "led.h"
#include "esp_log.h"
#include "led_indicator.h"

static const char *TAG = "led";

static const blink_step_t led_ready_step[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 50},
    {LED_BLINK_HOLD, LED_STATE_OFF, 50},
    {LED_BLINK_HOLD, LED_STATE_ON, 50},
    {LED_BLINK_HOLD, LED_STATE_OFF, 50},
    {LED_BLINK_HOLD, LED_STATE_ON, 50},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t led_usb_step[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t led_flash_step[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 200},
    {LED_BLINK_HOLD, LED_STATE_OFF, 200},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t led_error_step[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 1000},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0},
};

static blink_step_t const *led_indicator_blink_lists[] = {
    [LED_STATUS_ERROR] = led_error_step,
    [LED_STATUS_FLASH] = led_flash_step,
    [LED_STATUS_USB] = led_usb_step,
    [LED_STATUS_READY] = led_ready_step,
    [LED_STATUS_MAX] = NULL,
};

static led_indicator_handle_t led_handle = NULL;
static led_status_t latest = LED_STATUS_READY;

void led_init(void)
{
    led_indicator_gpio_config_t gpio_config = {
        .is_active_level_high = true,
        .gpio_num = CONFIG_BOARD_LED_GPIO,
    };
    led_indicator_config_t config = {
        .mode = LED_GPIO_MODE,
        .led_indicator_gpio_config = &gpio_config,
        .blink_lists = led_indicator_blink_lists,
        .blink_list_num = LED_STATUS_MAX,
    };
    led_handle = led_indicator_create(&config);
    if (led_handle == NULL) {
        ESP_LOGE(TAG, "Onboard led create failed");
    } else {
        led_indicator_start(led_handle, latest);
    }
}

void led_set_status(led_status_t status)
{
    if (led_handle == NULL || status == latest) {
        return;
    }
    led_indicator_stop(led_handle, latest);
    latest = status;
    led_indicator_start(led_handle, latest);
}
