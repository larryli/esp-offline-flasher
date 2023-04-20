#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "esp32_port.h"
#include "esp_err.h"
#include "esp_loader.h"
#include "esp_loader_io.h"
#include "esp_log.h"
#include "flash.h"

static const char *TAG = "flash";

// #define HIGHER_BAUDRATE 230400
#define HIGHER_BAUDRATE 0

static esp_loader_error_t connect_to_target(uint32_t higher_transmission_rate)
{
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    esp_loader_error_t err = esp_loader_connect(&connect_config);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Cannot connect to target. Error: %u", err);
        return err;
    }
    ESP_LOGI(TAG, "Connected to target");

    if (higher_transmission_rate && esp_loader_get_target() != ESP8266_CHIP) {
        err = esp_loader_change_transmission_rate(higher_transmission_rate);
        if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
            ESP_LOGE(
                TAG,
                "ESP8266 does not support change transmission rate command.");
            return err;
        } else if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Unable to change transmission rate on target.");
            return err;
        } else {
            err =
                loader_port_change_transmission_rate(higher_transmission_rate);
            if (err != ESP_LOADER_SUCCESS) {
                ESP_LOGE(TAG, "Unable to change transmission rate.");
                return err;
            }
            ESP_LOGI(TAG, "Transmission rate changed changed");
        }
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t flash_binary(const char *fname, size_t size,
                                       size_t address)
{
    esp_loader_error_t err;
    static uint8_t payload[1024];

    FILE *fp = fopen(fname, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Cannot open \"%s\" to read", fname);
        return ESP_LOADER_ERROR_FAIL;
    }

    ESP_LOGI(TAG, "Erasing flash (this may take a while)...");
    err = esp_loader_flash_start(address, size, sizeof(payload));
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Erasing flash failed with error %d", err);
        goto failed;
    }
    printf("Start programming");

    size_t binary_size = size;
    size_t written = 0;

    while (size > 0) {
        size_t read = fread(payload, 1, sizeof(payload), fp);
        if (read == 0) {
            printf("\n");
            ESP_LOGE(TAG, "Flash file is too small");
            goto failed;
        }
        size_t to_read = MIN(size, read);

        err = esp_loader_flash_write(payload, to_read);
        if (err != ESP_LOADER_SUCCESS) {
            printf("\n");
            ESP_LOGE(TAG, "Packet could not be written! Error %d", err);
            goto failed;
        }

        size -= to_read;
        written += to_read;

        int progress = (int)(((float)written / binary_size) * 100);
        printf("\rProgress: %d %%", progress);
        fflush(stdout);
    };

    printf("\rFinished programming\n");
    fclose(fp);

#ifdef CONFIG_SERIAL_FLASHER_MD5_ENABLED
    err = esp_loader_flash_verify();
    if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
        ESP_LOGW(TAG, "ESP8266 does not support flash verify command");
        return ESP_LOADER_SUCCESS;
    } else if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "MD5 does not match. err: %d", err);
        return err;
    }
    ESP_LOGI(TAG, "Flash verified");
#endif

    return ESP_LOADER_SUCCESS;

failed:
    fclose(fp);
    return err;
}

void flash(flash_args_t *args, flash_cb_t done)
{
    const loader_esp32_config_t config = {
        .baud_rate = 115200,
        .uart_port = CONFIG_FLASH_UART_PORT_NUM,
        .uart_rx_pin = CONFIG_FLASH_UART_RX_GPIO,
        .uart_tx_pin = CONFIG_FALSH_UART_TX_GPIO,
        .reset_trigger_pin = CONFIG_FLASH_UART_RESET_GPIO,
        .gpio0_trigger_pin = CONFIG_FLASH_UART_IO0_GPIO,
    };

    if (loader_port_esp32_init(&config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Serial initialization failed");
        if (done) {
            done(false);
        }
        return;
    }

    if (connect_to_target(HIGHER_BAUDRATE) != ESP_LOADER_SUCCESS) {
        goto failed;
    }
    if (esp_loader_get_target() != args->chip) {
        ESP_LOGE(TAG, "Target chip error: found %d, but flash %d",
                 esp_loader_get_target(), args->chip);
        goto failed;
    }
    for (int i = 0; i < args->flash_files_size; i++) {
        ESP_LOGI(TAG, "Flashing \"%s\" size: %ld, address: 0x%lX",
                 args->flash_files[i].path, args->flash_files[i].size,
                 args->flash_files[i].addr);
        if (flash_binary(args->flash_files[i].path, args->flash_files[i].size,
                         args->flash_files[i].addr) != ESP_LOADER_SUCCESS) {
            goto failed;
        }
    }

    ESP_LOGI(TAG, "Done!");
    loader_port_esp32_deinit();
    if (done) {
        done(true);
    }
    return;

failed:
    loader_port_esp32_deinit();
    if (done) {
        done(false);
    }
}
