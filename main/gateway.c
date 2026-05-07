/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#include "gateway.h"
#include "ui.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/lwip_napt.h"
#include "sdkconfig.h"

static const char *TAG = "patchy-gw";

static esp_netif_t *s_ap_netif = NULL;
static bool s_napt_on = false;
static bool s_napt_desired = false;

static void propagate_dns_to_ap(void)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta || !s_ap_netif) {
        return;
    }

    esp_netif_dns_info_t dns;
    if (esp_netif_get_dns_info(sta, ESP_NETIF_DNS_MAIN, &dns) != ESP_OK) {
        return;
    }

    esp_netif_dhcp_status_t st;
    esp_netif_dhcps_get_status(s_ap_netif, &st);
    if (st == ESP_NETIF_DHCP_STARTED) {
        esp_netif_dhcps_stop(s_ap_netif);
    }
    uint8_t dns_offer = 1;
    esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET,
                           ESP_NETIF_DOMAIN_NAME_SERVER, &dns_offer, sizeof(dns_offer));
    esp_netif_set_dns_info(s_ap_netif, ESP_NETIF_DNS_MAIN, &dns);
    esp_netif_dhcps_start(s_ap_netif);
}

static esp_err_t apply_ap_config(void)
{
    wifi_config_t ap_cfg = { 0 };
    const char *ssid = CONFIG_PATCHY_AP_SSID;
    const char *pw = CONFIG_PATCHY_AP_PASSWORD;
    strncpy((char *)ap_cfg.ap.ssid, ssid, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = strlen(ssid);
    ap_cfg.ap.channel = CONFIG_PATCHY_AP_CHANNEL;
    ap_cfg.ap.max_connection = CONFIG_PATCHY_AP_MAX_STA;
    if (strlen(pw) == 0) {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strncpy((char *)ap_cfg.ap.password, pw, sizeof(ap_cfg.ap.password) - 1);
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }
    return esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
}

// Try to enable NAPT now. Safe to call repeatedly: it bails when NAPT is
// already on, no longer wanted, or the AP netif isn't up yet (in which case
// the AP_START event handler will retry once the netif comes up).
static void try_enable_napt(void)
{
    if (!s_ap_netif || !s_napt_desired || s_napt_on) {
        return;
    }
    propagate_dns_to_ap();
    esp_err_t err = esp_netif_napt_enable(s_ap_netif);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "napt_enable deferred: %s", esp_err_to_name(err));
        return;
    }
    s_napt_on = true;
    patchy_ui_set_state(PATCHY_FORWARDING);
    ESP_LOGI(TAG, "NAPT enabled");
}

static void on_ap_start(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ESP_LOGI(TAG, "SoftAP started");
    try_enable_napt();
}

static esp_err_t bring_up_ap(void)
{
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_ap_netif) {
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START,
                    on_ap_start, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(apply_ap_config());

    propagate_dns_to_ap();

    ESP_LOGI(TAG, "SoftAP up: SSID=%s channel=%d",
             CONFIG_PATCHY_AP_SSID, CONFIG_PATCHY_AP_CHANNEL);
    return ESP_OK;
}

void patchy_gateway_on_sta_connected(void)
{
    if (!s_ap_netif) {
        ESP_ERROR_CHECK(bring_up_ap());
    } else {
        propagate_dns_to_ap();
    }
    patchy_gateway_set_forwarding(true);
}

void patchy_gateway_on_sta_disconnected(void)
{
    if (!s_ap_netif) {
        return;
    }
    s_napt_desired = false;
    if (s_napt_on) {
        esp_netif_napt_disable(s_ap_netif);
        s_napt_on = false;
        ESP_LOGI(TAG, "NAPT disabled (uplink lost)");
    }
    patchy_ui_set_state(PATCHY_AP_ONLY);
}

esp_err_t patchy_gateway_set_forwarding(bool on)
{
    if (!s_ap_netif) {
        ESP_LOGW(TAG, "set_forwarding(%d): AP not up yet", on);
        return ESP_ERR_INVALID_STATE;
    }
    if (on) {
        s_napt_desired = true;
        try_enable_napt();
    } else if (s_napt_on) {
        s_napt_desired = false;
        esp_netif_napt_disable(s_ap_netif);
        s_napt_on = false;
        patchy_ui_set_state(PATCHY_SIMULATED_DOWN);
        ESP_LOGI(TAG, "NAPT disabled (simulated outage)");
    } else {
        s_napt_desired = false;
    }
    return ESP_OK;
}

esp_err_t patchy_gateway_set_ap_enabled(bool on)
{
    if (!s_ap_netif) {
        ESP_LOGW(TAG, "set_ap_enabled(%d): AP not up yet", on);
        return ESP_ERR_INVALID_STATE;
    }
    if (on) {
        // Mark NAPT as wanted; the actual enable will happen when WIFI_EVENT_AP_START
        // fires (set_mode is async, so the AP netif isn't yet up here).
        s_napt_desired = true;
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set_mode(APSTA) failed: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "SoftAP restoring (NAPT will re-enable on AP_START)");
    } else {
        s_napt_desired = false;
        if (s_napt_on) {
            esp_netif_napt_disable(s_ap_netif);
            s_napt_on = false;
        }
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set_mode(STA) failed: %s", esp_err_to_name(err));
            return err;
        }
        patchy_ui_set_state(PATCHY_SIMULATED_DOWN);
        ESP_LOGI(TAG, "SoftAP destroyed (simulated outage)");
    }
    return ESP_OK;
}

bool patchy_gateway_is_up(void)
{
    return s_ap_netif != NULL;
}
