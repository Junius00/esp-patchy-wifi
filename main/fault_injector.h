/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FAULT_OFF,
    FAULT_MANUAL_DOWN,   // user toggled simulated disconnect on
    FAULT_JITTERY,       // random outages driven by timer
    FAULT_CYCLICAL,      // N cycles of (down_s, up_s) driven by timer
} patchy_fault_mode_t;

// Mechanism used to inject a simulated outage.
typedef enum {
    INJECT_NAPT,    // toggle NAPT forwarding on the SoftAP (default)
    INJECT_SOFTAP,  // tear down the SoftAP entirely (Wi-Fi network disappears)
} patchy_inject_mode_t;

esp_err_t patchy_fault_init(void);

// Button-wired handlers:
void patchy_fault_short_press(void);
void patchy_fault_long_press(void);   // ~1s hold-and-release: toggle jittery
void patchy_fault_reset(void);        // >=5s hold: wipe creds + reboot

patchy_fault_mode_t patchy_fault_get_mode(void);

// CLI-facing setters. Each returns ESP_ERR_INVALID_STATE if gateway not up,
// or ESP_ERR_INVALID_ARG for bad params.
esp_err_t patchy_fault_manual_set(bool down);
esp_err_t patchy_fault_jittery_set(bool enable);
esp_err_t patchy_fault_cycle_start(uint32_t n, uint32_t down_s, uint32_t up_s);
esp_err_t patchy_fault_stop(void);

patchy_inject_mode_t patchy_fault_get_inject_mode(void);

// Switch the fault-injection mechanism. Rejected with ESP_ERR_INVALID_STATE
// if a fault is currently being injected (i.e. mode != FAULT_OFF), so callers
// must `stop` first.
esp_err_t patchy_fault_set_inject_mode(patchy_inject_mode_t mode);
