#include "btn.h"
#include "esp_log.h"
#include "iot_button.h"

static const char *TAG = "btn";

btn_cb_t click_cb = NULL, double_click_cb = NULL, press_cb = NULL;
static void btn_cb(void *arg, void *usr_data)
{
    button_event_t evt = iot_button_get_event((button_handle_t)arg);
    switch (evt) {
    case BUTTON_SINGLE_CLICK:
        if (click_cb != NULL) {
            click_cb();
        }
        break;
    case BUTTON_DOUBLE_CLICK:
        if (double_click_cb != NULL) {
            double_click_cb();
        }
        break;
    case BUTTON_LONG_PRESS_START:
        if (press_cb != NULL) {
            press_cb();
        }
        break;
    default:
        ESP_LOGE(TAG, "Wrong event type: %d", evt);
        break;
    }
}

void btn_init(btn_cb_t click, btn_cb_t double_click, btn_cb_t press)
{
    button_config_t gpio_btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config =
            {
                .gpio_num = CONFIG_BOARD_BUTTON_GPIO,
                .active_level = 0,
            },
    };
    button_handle_t btn_handle = iot_button_create(&gpio_btn_cfg);
    if (btn_handle == NULL) {
        ESP_LOGE(TAG, "Onboard button create failed");
    } else {
        click_cb = click;
        double_click_cb = double_click;
        press_cb = press;

        iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, btn_cb, NULL);
        iot_button_register_cb(btn_handle, BUTTON_DOUBLE_CLICK, btn_cb, NULL);
        iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, btn_cb,
                               NULL);
    }
}
