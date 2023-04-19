#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "esp_log.h"
#include "flash_args.h"

static const char *TAG = "flash_args";

static target_chip_t parse_chip(char *str)
{
    if (strncmp("esp", str, 3) == 0) {
        if (strncmp("32", str + 3, 2) == 0) {
            switch (str[5]) {
            case '\0':
                return ESP32_CHIP;
            case 'c':
                if (strcmp("2", str + 6) == 0) {
                    return ESP32C2_CHIP;
                } else if (strcmp("3", str + 6) == 0) {
                    return ESP32C3_CHIP;
                }
                break;
            case 's':
                if (strcmp("2", str + 6) == 0) {
                    return ESP32S2_CHIP;
                } else if (strcmp("3", str + 6) == 0) {
                    return ESP32S3_CHIP;
                }
                break;
            case 'h':
                if (strcmp("2", str + 6) == 0) {
                    return ESP32H2_CHIP;
                } else if (strcmp("4", str + 6) == 0) {
                    return ESP32H4_CHIP;
                }
                break;
            default:
                break;
            }
        } else if (strcmp("8266", str + 3) == 0) {
            return ESP8266_CHIP;
        }
    }
    return ESP_UNKNOWN_CHIP;
}

static const char *dump_chip(target_chip_t chip)
{
    switch (chip) {
    case ESP8266_CHIP:
        return "esp8266";
    case ESP32_CHIP:
        return "esp32";
    case ESP32S2_CHIP:
        return "esp32s2";
    case ESP32C3_CHIP:
        return "esp32c3";
    case ESP32S3_CHIP:
        return "esp32s3";
    case ESP32C2_CHIP:
        return "esp32c2";
    case ESP32H4_CHIP:
        return "esp32h4";
    case ESP32H2_CHIP:
        return "esp32h2";
    default:
        return "UNKNOWN";
    }
}

flash_args_t *flash_args_from_json(const char *json, uint32_t length,
                                   const char *base_path)
{
    flash_args_t *args = NULL;
    size_t base_path_len = strlen(base_path) + 2; // "{$base_path}/{$path}\0"
    cJSON *root = cJSON_ParseWithLength(json, length);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Read json error: %s\n", error_ptr);
        }
        goto failed;
    }

    const cJSON *flash_files = cJSON_GetObjectItem(root, "flash_files");
    if (flash_files == NULL) {
        ESP_LOGE(TAG, "Cannot find \"flash_files\" arg\n");
        goto failed;
    }
    int size = cJSON_GetArraySize(flash_files);
    if (size <= 0) {
        ESP_LOGE(TAG, "Invalid \"flash_files\" size\n");
        goto failed;
    }
    size_t s = sizeof(flash_args_t) + sizeof(flash_file_t) * size;
    args = malloc(s);
    if (args == NULL) {
        ESP_LOGE(TAG, "Malloc flash_args %d bytes failed\n", s);
        goto failed;
    }
    memset(args, 0, s);
    args->chip = ESP_UNKNOWN_CHIP;
    args->flash_files_size = size;
    int n = 0;
    for (const cJSON *i = flash_files->child; i != NULL; i = i->next) {
        size_t s = base_path_len + strlen(i->valuestring);
        char *path = malloc(s);
        if (path == NULL) {
            ESP_LOGE(TAG,
                     "Malloc flash_args.flash_files[%d].path %d bytes "
                     "failed\n",
                     n, s);
            goto failed;
        }
        strcpy(path, base_path);
        strcat(path, "/");
        strcat(path, i->valuestring);
        struct stat st;
        if (stat(path, &st) == 0) {
            args->flash_files[n].size = st.st_size;
        } else {
            ESP_LOGE(TAG, "Flash file \"%s\" is not exists\n", path);
            goto failed;
        }
        args->flash_files[n].addr = strtoul(i->string, NULL, 0);
        args->flash_files[n].path = path;
        n++;
    }
    const cJSON *extra = cJSON_GetObjectItem(root, "extra_esptool_args");
    if (extra != NULL) {
        const cJSON *chip = cJSON_GetObjectItem(extra, "chip");
        if (chip != NULL) {
            args->chip = parse_chip(chip->valuestring);
        }
    }

    cJSON_Delete(root);
    return args;

failed:
    flash_args_free(args);
    cJSON_Delete(root);
    return NULL;
}

void flash_args_free(flash_args_t *args)
{
    if (args == NULL) {
        return;
    }
    for (int i = 0; i < args->flash_files_size; i++) {
        if (args->flash_files[i].path != NULL) {
            free(args->flash_files[i].path);
        }
    }
    free(args);
}

void flash_args_dump(flash_args_t *args)
{
    if (args == NULL) {
        return;
    }
    ESP_LOGI(TAG, "Flash chip: %s(%d)", dump_chip(args->chip), args->chip);
    ESP_LOGI(TAG, "Flash %d file(s):", args->flash_files_size);
    for (int i = 0; i < args->flash_files_size; i++) {
        printf("  - \033[1;37maddr\033[0m: \033[1;36m0x%lx\033[0m\n"
               "    \033[1;37msize\033[0m: \033[1;36m%ld\033[0m\n"
               "    \033[1;37mpath\033[0m: \033[1;32m%s\033[0m\n",
               args->flash_files[i].addr, args->flash_files[i].size,
               args->flash_files[i].path);
    }
}
