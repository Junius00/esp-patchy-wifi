/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

typedef enum {
    FAULT_OFF,
    FAULT_MANUAL_DOWN,   // user toggled simulated disconnect on
    FAULT_JITTERY,       // random outages driven by timer
} patchy_fault_mode_t;

esp_err_t patchy_fault_init(void);

// Button-wired handlers:
void patchy_fault_short_press(void);
void patchy_fault_long_press(void);   // ~1s hold-and-release: toggle jittery
void patchy_fault_reset(void);        // >=5s hold: wipe creds + reboot

patchy_fault_mode_t patchy_fault_get_mode(void);
