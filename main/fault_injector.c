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

// cyclical state
static uint32_t s_cycles_remaining;
static uint32_t s_cycle_down_s;
static uint32_t s_cycle_up_s;
static bool     s_cycle_phase_down;

static uint32_t rand_seconds(int min_s, int max_s)
{
    if (max_s <= min_s) {
        return (uint32_t)min_s;
    }
    uint32_t span = (uint32_t)(max_s - min_s + 1);
    return (uint32_t)min_s + (esp_random() % span);
}

static void schedule_jitter_next(bool next_is_down);

static void jittery_tick(void)
{
    bool currently_forwarding = (patchy_ui_get_state() == PATCHY_FORWARDING);
    if (currently_forwarding) {
        patchy_gateway_set_forwarding(false);
        schedule_jitter_next(false);   // next transition: back up
    } else {
        patchy_gateway_set_forwarding(true);
        schedule_jitter_next(true);    // next transition: go down
    }
}

// next_is_down=true means: next tick will take us DOWN, so schedule the
// uptime interval we're currently spending online.
static void schedule_jitter_next(bool next_is_down)
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

static void cyclical_tick(void)
{
    if (s_cycle_phase_down) {
        // downtime just elapsed → bring up, schedule uptime
        patchy_gateway_set_forwarding(true);
        s_cycle_phase_down = false;
        ESP_LOGI(TAG, "cycle: up phase, %u s (cycles left after this: %u)",
                 (unsigned)s_cycle_up_s, (unsigned)(s_cycles_remaining - 1));
        esp_timer_start_once(s_timer, (uint64_t)s_cycle_up_s * 1000000ULL);
    } else {
        // uptime just elapsed → cycle done
        s_cycles_remaining--;
        if (s_cycles_remaining == 0) {
            ESP_LOGI(TAG, "cycle: complete");
            s_mode = FAULT_OFF;
            return;
        }
        patchy_gateway_set_forwarding(false);
        s_cycle_phase_down = true;
        ESP_LOGI(TAG, "cycle: down phase, %u s (%u cycles left)",
                 (unsigned)s_cycle_down_s, (unsigned)s_cycles_remaining);
        esp_timer_start_once(s_timer, (uint64_t)s_cycle_down_s * 1000000ULL);
    }
}

static void timer_tick(void *arg)
{
    if (s_mode == FAULT_JITTERY) {
        jittery_tick();
    } else if (s_mode == FAULT_CYCLICAL) {
        cyclical_tick();
    }
}

esp_err_t patchy_fault_init(void)
{
    const esp_timer_create_args_t args = {
        .callback = timer_tick,
        .name = "patchy-fault",
    };
    return esp_timer_create(&args, &s_timer);
}

patchy_fault_mode_t patchy_fault_get_mode(void)
{
    return s_mode;
}

esp_err_t patchy_fault_manual_set(bool down)
{
    if (!patchy_gateway_is_up()) {
        ESP_LOGW(TAG, "manual: gateway not up");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mode == FAULT_JITTERY || s_mode == FAULT_CYCLICAL) {
        ESP_LOGW(TAG, "manual: timed mode active, stop it first");
        return ESP_ERR_INVALID_STATE;
    }
    if (down) {
        patchy_gateway_set_forwarding(false);
        s_mode = FAULT_MANUAL_DOWN;
        ESP_LOGI(TAG, "manual disconnect");
    } else {
        patchy_gateway_set_forwarding(true);
        s_mode = FAULT_OFF;
        ESP_LOGI(TAG, "manual reconnect");
    }
    return ESP_OK;
}

esp_err_t patchy_fault_jittery_set(bool enable)
{
    if (!patchy_gateway_is_up()) {
        ESP_LOGW(TAG, "jittery: gateway not up");
        return ESP_ERR_INVALID_STATE;
    }
    if (enable) {
        if (s_mode == FAULT_CYCLICAL) {
            ESP_LOGW(TAG, "jittery: cyclical active, stop it first");
            return ESP_ERR_INVALID_STATE;
        }
        if (s_mode == FAULT_JITTERY) {
            return ESP_OK;
        }
        if (s_mode == FAULT_MANUAL_DOWN) {
            patchy_gateway_set_forwarding(true);
        }
        s_mode = FAULT_JITTERY;
        ESP_LOGI(TAG, "jittery ON");
        schedule_jitter_next(true);
    } else {
        if (s_mode != FAULT_JITTERY) {
            return ESP_OK;
        }
        esp_timer_stop(s_timer);
        patchy_gateway_set_forwarding(true);
        s_mode = FAULT_OFF;
        ESP_LOGI(TAG, "jittery OFF");
    }
    return ESP_OK;
}

esp_err_t patchy_fault_cycle_start(uint32_t n, uint32_t down_s, uint32_t up_s)
{
    if (n == 0 || down_s == 0 || up_s == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!patchy_gateway_is_up()) {
        ESP_LOGW(TAG, "cycle: gateway not up");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mode == FAULT_JITTERY || s_mode == FAULT_CYCLICAL) {
        ESP_LOGW(TAG, "cycle: timed mode active, stop it first");
        return ESP_ERR_INVALID_STATE;
    }

    s_cycles_remaining = n;
    s_cycle_down_s = down_s;
    s_cycle_up_s = up_s;
    s_cycle_phase_down = true;
    s_mode = FAULT_CYCLICAL;

    patchy_gateway_set_forwarding(false);
    ESP_LOGI(TAG, "cycle: starting %u cycles, down %u s / up %u s",
             (unsigned)n, (unsigned)down_s, (unsigned)up_s);
    esp_timer_start_once(s_timer, (uint64_t)down_s * 1000000ULL);
    return ESP_OK;
}

esp_err_t patchy_fault_stop(void)
{
    if (!patchy_gateway_is_up()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_timer) {
        esp_timer_stop(s_timer);
    }
    patchy_gateway_set_forwarding(true);
    s_mode = FAULT_OFF;
    ESP_LOGI(TAG, "fault stopped");
    return ESP_OK;
}

void patchy_fault_short_press(void)
{
    if (!patchy_gateway_is_up()) {
        ESP_LOGW(TAG, "short press ignored: gateway not up");
        return;
    }
    if (s_mode == FAULT_JITTERY || s_mode == FAULT_CYCLICAL) {
        ESP_LOGI(TAG, "short press ignored in timed mode");
        return;
    }
    patchy_fault_manual_set(s_mode != FAULT_MANUAL_DOWN);
}

void patchy_fault_long_press(void)
{
    if (!patchy_gateway_is_up()) {
        ESP_LOGW(TAG, "long press ignored: gateway not up");
        return;
    }
    if (s_mode == FAULT_CYCLICAL) {
        ESP_LOGI(TAG, "long press ignored in cyclical mode");
        return;
    }
    patchy_fault_jittery_set(s_mode != FAULT_JITTERY);
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
