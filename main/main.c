/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#include "esp_err.h"
#include "esp_log.h"

#include "console.h"
#include "fault_injector.h"
#include "provisioning.h"
#include "ui.h"

static const char *TAG = "patchy";

void app_main(void)
{
    ESP_LOGI(TAG, "booting esp-patchy-wifi");

    const patchy_ui_callbacks_t ui_cbs = {
        .on_short_press  = patchy_fault_short_press,
        .on_jittery_hold = patchy_fault_long_press,
        .on_reset_hold   = patchy_fault_reset,
    };
    ESP_ERROR_CHECK(patchy_ui_init(&ui_cbs));
    ESP_ERROR_CHECK(patchy_fault_init());
    ESP_ERROR_CHECK(patchy_prov_start());
    ESP_ERROR_CHECK(patchy_console_start());
}
