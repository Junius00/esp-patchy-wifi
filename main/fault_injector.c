/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#include "fault_injector.h"
#include "gateway.h"
#include "provisioning.h"
#include "ui.h"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "patchy-fault";

static patchy_fault_mode_t s_mode = FAULT_OFF;
static esp_timer_handle_t s_timer = NULL;

static uint32_t rand_seconds(int min_s, int max_s)
{
    if (max_s <= min_s) {
        return (uint32_t)min_s;
    }
    uint32_t span = (uint32_t)(max_s - min_s + 1);
    return (uint32_t)min_s + (esp_random() % span);
}

static void schedule_next(bool next_is_down);

static void jittery_tick(void *arg)
{
    if (s_mode != FAULT_JITTERY) {
        return;
    }

    bool currently_forwarding = (patchy_ui_get_state() == PATCHY_FORWARDING);
    if (currently_forwarding) {
        patchy_gateway_set_forwarding(false);
        schedule_next(false);   // next transition: back up
    } else {
        patchy_gateway_set_forwarding(true);
        schedule_next(true);    // next transition: go down
    }
}

// next_is_down=true means: next tick will take us DOWN, so schedule the
// uptime interval we're currently spending online.
static void schedule_next(bool next_is_down)
{
    uint32_t secs;
    if (next_is_down) {
        secs = rand_seconds(CONFIG_PATCHY_JITTER_UPTIME_MIN_S,
                            CONFIG_PATCHY_JITTER_UPTIME_MAX_S);
        ESP_LOGI(TAG, "jittery: online for %u s", (unsigned)secs);
    } else {
        secs = rand_seconds(CONFIG_PATCHY_JITTER_DOWNTIME_MIN_S,
                            CONFIG_PATCHY_JITTER_DOWNTIME_MAX_S);
        ESP_LOGI(TAG, "jittery: offline for %u s", (unsigned)secs);
    }
    esp_timer_stop(s_timer);
    esp_timer_start_once(s_timer, (uint64_t)secs * 1000000ULL);
}

esp_err_t patchy_fault_init(void)
{
    const esp_timer_create_args_t args = {
        .callback = jittery_tick,
        .name = "patchy-jitter",
    };
    return esp_timer_create(&args, &s_timer);
}

patchy_fault_mode_t patchy_fault_get_mode(void)
{
    return s_mode;
}

void patchy_fault_short_press(void)
{
    if (!patchy_gateway_is_up()) {
        ESP_LOGW(TAG, "short press ignored: gateway not up");
        return;
    }
    if (s_mode == FAULT_JITTERY) {
        ESP_LOGI(TAG, "short press ignored in JITTERY mode");
        return;
    }

    if (s_mode == FAULT_MANUAL_DOWN) {
        patchy_gateway_set_forwarding(true);
        s_mode = FAULT_OFF;
        ESP_LOGI(TAG, "manual reconnect");
    } else {
        patchy_gateway_set_forwarding(false);
        s_mode = FAULT_MANUAL_DOWN;
        ESP_LOGI(TAG, "manual disconnect");
    }
}

void patchy_fault_long_press(void)
{
    if (!patchy_gateway_is_up()) {
        ESP_LOGW(TAG, "long press ignored: gateway not up");
        return;
    }

    if (s_mode == FAULT_JITTERY) {
        esp_timer_stop(s_timer);
        patchy_gateway_set_forwarding(true);
        s_mode = FAULT_OFF;
        ESP_LOGI(TAG, "jittery OFF");
    } else {
        if (s_mode == FAULT_MANUAL_DOWN) {
            patchy_gateway_set_forwarding(true);
        }
        s_mode = FAULT_JITTERY;
        ESP_LOGI(TAG, "jittery ON");
        schedule_next(true);
    }
}

void patchy_fault_reset(void)
{
    ESP_LOGW(TAG, "network reset requested — erasing creds and rebooting");
    if (s_timer) {
        esp_timer_stop(s_timer);
    }
    // Flash LED white briefly so the user sees the reset took effect.
    patchy_ui_set_state(PATCHY_PRE_PROV);
    patchy_prov_reset();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}
