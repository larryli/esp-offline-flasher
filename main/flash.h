#pragma once

#include "flash_args.h"

typedef void (*flash_cb_t)(bool);

void flash(flash_args_t *args, flash_cb_t done);
