#pragma once

typedef enum {
    LED_STATUS_ERROR,
    LED_STATUS_FLASH,
    LED_STATUS_USB,
    LED_STATUS_READY,
    LED_STATUS_MAX,
} led_status_t;

void led_init(void);
void led_set_status(led_status_t status);
