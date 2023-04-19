#pragma once

typedef void (*btn_cb_t)(void);

void btn_init(btn_cb_t click, btn_cb_t double_click, btn_cb_t press);
