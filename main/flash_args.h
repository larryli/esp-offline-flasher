#pragma once

#include <stdint.h>

#include "esp_loader.h"

typedef struct {
    uint32_t addr;
    char *path;
    uint32_t size;
} flash_file_t;

typedef struct {
    target_chip_t chip;
    int flash_files_size;
    flash_file_t flash_files[];
} flash_args_t;

flash_args_t *flash_args_find(char *dir);
void flash_args_free(flash_args_t *args);
void flash_args_dump(flash_args_t *args);
