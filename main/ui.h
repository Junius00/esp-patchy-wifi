/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

typedef enum {
    PATCHY_PRE_PROV,         // white  — no STA, AP not up (BLE advertising)
    PATCHY_AP_ONLY,          // red    — AP up, STA not connected
    PATCHY_FORWARDING,       // green  — STA connected, NAPT enabled
    PATCHY_SIMULATED_DOWN,   // blue   — STA connected, NAPT disabled
} patchy_state_t;

typedef void (*patchy_button_cb_t)(void);

typedef struct {
    patchy_button_cb_t on_short_press;
    patchy_button_cb_t on_jittery_hold;   // ~1s press and release
    patchy_button_cb_t on_reset_hold;     // >=5s hold
} patchy_ui_callbacks_t;

esp_err_t patchy_ui_init(const patchy_ui_callbacks_t *callbacks);

void patchy_ui_set_state(patchy_state_t state);

patchy_state_t patchy_ui_get_state(void);
