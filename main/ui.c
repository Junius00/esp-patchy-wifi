/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui.h"

#include "app_led.h"
#include "esp_log.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "sdkconfig.h"

static const char *TAG = "patchy-ui";

static patchy_state_t s_state = PATCHY_PRE_PROV;
static patchy_ui_callbacks_t s_cbs;
static button_handle_t s_btn;

#define COLOR_LEVEL CONFIG_PATCHY_LED_BRIGHTNESS

static void apply_color(patchy_state_t state)
{
    app_led_color_rgb_t c = { 0, 0, 0 };
    switch (state) {
    case PATCHY_PRE_PROV:       c = (app_led_color_rgb_t) {
            COLOR_LEVEL, COLOR_LEVEL, COLOR_LEVEL
        }; break;
    case PATCHY_AP_ONLY:        c = (app_led_color_rgb_t) {
            COLOR_LEVEL, 0, 0
        };     break;
    case PATCHY_FORWARDING:     c = (app_led_color_rgb_t) {
            0, COLOR_LEVEL, 0
        };     break;
    case PATCHY_SIMULATED_DOWN: c = (app_led_color_rgb_t) {
            0, 0, COLOR_LEVEL
        };     break;
    }
    app_led_set_color_rgb(c);
    app_led_set_power(true);
}

void patchy_ui_set_state(patchy_state_t state)
{
    s_state = state;
    ESP_LOGI(TAG, "state -> %d", state);
    apply_color(state);
}

patchy_state_t patchy_ui_get_state(void)
{
    return s_state;
}

static void short_press_cb(void *arg, void *data)
{
    if (s_cbs.on_short_press) {
        s_cbs.on_short_press();
    }
}

static void jittery_hold_cb(void *arg, void *data)
{
    if (s_cbs.on_jittery_hold) {
        s_cbs.on_jittery_hold();
    }
}

static void reset_hold_cb(void *arg, void *data)
{
    if (s_cbs.on_reset_hold) {
        s_cbs.on_reset_hold();
    }
}

// LED indicators to preview which action will fire on release.
// Solid yellow = release now toggles jittery mode.
// Solid magenta = kept holding past reset threshold; reset is firing.
static void jittery_arm_indicator_cb(void *arg, void *data)
{
    app_led_color_rgb_t yellow = { COLOR_LEVEL, (uint8_t)(COLOR_LEVEL * 2 / 3), 0 };
    app_led_set_color_rgb(yellow);
    app_led_set_power(true);
    ESP_LOGI(TAG, "hold indicator: release now -> toggle jittery");
}

static void press_up_cb(void *arg, void *data)
{
    // Restore the LED to whatever the logical state is. Covers the case
    // where the jittery-arm indicator lit up but the user released before
    // the reset threshold fired.
    apply_color(s_state);
}

esp_err_t patchy_ui_init(const patchy_ui_callbacks_t *callbacks)
{
    if (!callbacks) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cbs = *callbacks;

    esp_err_t err = app_led_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "app_led_init failed: %s", esp_err_to_name(err));
        return err;
    }
    patchy_ui_set_state(PATCHY_PRE_PROV);

    button_config_t btn_cfg = { 0 };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = CONFIG_PATCHY_BUTTON_GPIO,
        .active_level = CONFIG_PATCHY_BUTTON_ACTIVE_LEVEL,
    };
    err = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &s_btn);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "iot_button_new_gpio_device failed: %s", esp_err_to_name(err));
        return err;
    }

    iot_button_register_cb(s_btn, BUTTON_SINGLE_CLICK, NULL, short_press_cb, NULL);

    button_event_args_t jittery_args = {
        .long_press.press_time = CONFIG_PATCHY_JITTERY_HOLD_MS,
    };
    // Light the yellow preview the moment the press crosses the jittery
    // threshold — tells the user "let go now to toggle jittery".
    iot_button_register_cb(s_btn, BUTTON_LONG_PRESS_START, &jittery_args,
                           jittery_arm_indicator_cb, NULL);
    // Actual jittery toggle fires on release, but only if still under the
    // reset threshold (since reset reboots the device before this can run).
    iot_button_register_cb(s_btn, BUTTON_LONG_PRESS_UP, &jittery_args, jittery_hold_cb, NULL);

    button_event_args_t reset_args = {
        .long_press.press_time = CONFIG_PATCHY_RESET_HOLD_MS,
    };
    iot_button_register_cb(s_btn, BUTTON_LONG_PRESS_START, &reset_args, reset_hold_cb, NULL);

    // Any release restores the LED to its logical state (covers the
    // "yellow preview but released before reset" path, and sub-1s clicks).
    iot_button_register_cb(s_btn, BUTTON_PRESS_UP, NULL, press_up_cb, NULL);

    ESP_LOGI(TAG, "UI ready (btn GPIO %d, active=%d)",
             CONFIG_PATCHY_BUTTON_GPIO, CONFIG_PATCHY_BUTTON_ACTIVE_LEVEL);
    return ESP_OK;
}
