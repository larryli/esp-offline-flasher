#pragma once

#include "tusb_msc_storage.h"

typedef void (*usb_cb_t)(bool);

void usb_init(usb_cb_t chg);

bool inline usb_mounted(void)
{
    return tinyusb_msc_storage_in_use_by_usb_host();
}
