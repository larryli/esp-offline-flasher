#pragma once

#include <stdint.h>

typedef struct {
    char *buf;
    uint32_t size;
    char *dir;
} findfile_t;

findfile_t *findfile(const char *dir, const char *fname);
