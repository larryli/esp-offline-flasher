#include "usb.h"
#include "esp_check.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"

#ifdef CONFIG_ENABLE_CDC
#include "driver/gpio.h"
#include "esp_private/periph_ctrl.h"
#include "soc/rtc_cntl_reg.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"
#if CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/usb/chip_usb_dw_wrapper.h"
#include "esp32s2/rom/usb/usb_dc.h"
#include "esp32s2/rom/usb/usb_persist.h"
#endif
#endif

static const char *TAG = "usb";

static usb_cb_t chg_cb = NULL;

/* TinyUSB descriptors ********************************* */
#ifdef CONFIG_ENABLE_CDC
#define TUSB_DESC_TOTAL_LEN                                                    \
    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN)
#else
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)
#endif

enum {
#ifdef CONFIG_ENABLE_CDC
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_MSC,
#else
    ITF_NUM_MSC = 0,
#endif
    ITF_NUM_TOTAL
};

enum {
    EP_EMPTY = 0,
#ifdef CONFIG_ENABLE_CDC
    EPNUM_0_CDC_NOTIF,
    EPNUM_0_CDC,
#endif
    EPNUM_MSC,
};

static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

#ifdef CONFIG_ENABLE_CDC
    // Interface number, string index, EP notification address and size, EP data
    // address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, 0x80 | EPNUM_0_CDC_NOTIF, 8, EPNUM_0_CDC,
                       0x80 | EPNUM_0_CDC, CFG_TUD_CDC_EP_BUFSIZE),
#endif

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC, 0x80 | EPNUM_MSC,
                       TUD_OPT_HIGH_SPEED ? 512 : 64),
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_ESPRESSIF_VID, // This is Espressif VID. This needs to be
                                   // changed according to Users / Customers
    .idProduct = 0x4003,
    .bcdDevice = CONFIG_TINYUSB_DESC_BCD_DEVICE,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

static char const *string_desc_arr[] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING, // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,      // 2: Product
    CONFIG_TINYUSB_DESC_SERIAL_STRING,       // 3: Serials, should use chip ID
#ifdef CONFIG_ENABLE_CDC
    CONFIG_TINYUSB_DESC_CDC_STRING, // 4: CDC Interface
#endif
    CONFIG_TINYUSB_DESC_MSC_STRING, // 5: MSC Interface
    NULL // NULL: Must be last. Indicates end of array
};
/********************************* TinyUSB descriptors */

// mount the partition and show all the files in BASE_PATH
static void _mount(void)
{
    ESP_LOGI(TAG, "Mount storage...");
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(CONFIG_TINYUSB_MSC_MOUNT_PATH));
    return;
}

static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL) {
        ESP_LOGE(TAG,
                 "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}

static void storage_mount_changed(tinyusb_msc_event_t *event)
{
    if (chg_cb != NULL) {
        chg_cb(event->mount_changed_data.is_mounted);
    }
}

#ifdef CONFIG_ENABLE_CDC
static void IRAM_ATTR usb_persist_shutdown_handler(void)
{
#if CONFIG_IDF_TARGET_ESP32S2
    periph_module_reset(PERIPH_USB_MODULE);
    periph_module_enable(PERIPH_USB_MODULE);
#endif
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
}

static void usb_persist_restart(void)
{
    if (esp_register_shutdown_handler(usb_persist_shutdown_handler) == ESP_OK) {
        esp_restart();
    }
}

static void tinyusb_cdc_line_state_changed_callback(int itf,
                                                    cdcacm_event_t *event)
{
    static int lineState = 0;
    static bool dtr = false, rts = false;

    if (dtr == event->line_state_changed_data.dtr &&
        rts == event->line_state_changed_data.rts) {
        return; // Skip duplicate events
    }
    dtr = event->line_state_changed_data.dtr;
    rts = event->line_state_changed_data.rts;

    if (!dtr && rts) {
        if (lineState == 0) {
            lineState++;
        } else {
            lineState = 0;
        }
    } else if (dtr && rts) {
        if (lineState == 1) {
            lineState++;
        } else {
            lineState = 0;
        }
    } else if (dtr && !rts) {
        if (lineState == 2) {
            lineState++;
        } else {
            lineState = 0;
        }
    } else if (!dtr && !rts) {
        if (lineState == 1) {
            esp_restart(); // Monitor
        } else if (lineState == 3) {
            usb_persist_restart(); // Flash
        } else {
            lineState = 0;
        }
    }
}
#endif

void usb_init(usb_cb_t chg)
{
    ESP_LOGI(TAG, "Initializing storage...");

    static wl_handle_t wl_handle = WL_INVALID_HANDLE;
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));
    chg_cb = chg;

    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle = wl_handle,
        .callback_mount_changed = storage_mount_changed,
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));

    // mounted in the app by default
    _mount();

    ESP_LOGI(TAG, "USB initialization");
    const tinyusb_config_t tusb_cfg = {
        .self_powered = false,
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count =
            sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy = false,
        .configuration_descriptor = desc_configuration,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
#ifdef CONFIG_ENABLE_CDC
    tinyusb_config_cdcacm_t acm_cfg = {
        .rx_unread_buf_sz = CONFIG_TINYUSB_CDC_TX_BUFSIZE,
        .callback_line_state_changed = tinyusb_cdc_line_state_changed_callback,
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));

    esp_tusb_init_console(TINYUSB_CDC_ACM_0); // log to usb
#endif
    ESP_LOGI(TAG, "USB initialization DONE");
}
