/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#include "provisioning.h"
#include "gateway.h"
#include "ui.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "app_network.h"

static const char *TAG = "patchy-prov";

static bool s_provisioned = false;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA disconnected");
        patchy_gateway_on_sta_disconnected();
        // rmaker_app_network's internal handler triggers reconnect.
    }
}

static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "STA got IP " IPSTR, IP2STR(&evt->ip_info.ip));
        patchy_gateway_on_sta_connected();
    }
}

bool patchy_prov_is_provisioned(void)
{
    return s_provisioned;
}

esp_err_t patchy_prov_reset(void)
{
    ESP_LOGW(TAG, "wiping provisioning state (NVS erase)");
    return nvs_flash_erase();
}

esp_err_t patchy_prov_start(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // app_network_init() creates default event loop, inits esp_netif, creates
    // the default Wi-Fi STA netif, calls esp_wifi_init(), and registers its
    // own IP_EVENT_STA_GOT_IP + NETWORK_PROV_EVENT handlers.
    app_network_init();

    ESP_ERROR_CHECK(app_network_set_custom_pop(CONFIG_PATCHY_PROV_POP));

    ESP_LOGI(TAG, "starting provisioning (will block until STA has IP)");
    err = app_network_start(POP_TYPE_CUSTOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "app_network_start failed: %s", esp_err_to_name(err));
        return err;
    }
    s_provisioned = true;
    ESP_LOGI(TAG, "network connected");

    // Register our own handlers AFTER provisioning is done so we don't
    // interfere with the manager's internal reconnection logic. These only
    // drive LED state + gateway (re-)setup on reconnect.
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                    &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &ip_event_handler, NULL));

    // app_network_start blocked until we got IP. Bring up the gateway now.
    patchy_gateway_on_sta_connected();

    return ESP_OK;
}
