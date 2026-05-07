/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

// Called from the IP_EVENT_STA_GOT_IP handler. On first call brings up the
// SoftAP, switches to WIFI_MODE_APSTA, enables NAPT, propagates the STA's
// DNS to the AP's DHCPS. Subsequent calls re-enable NAPT after uplink
// reconnects.
void patchy_gateway_on_sta_connected(void);

// Called when the STA loses its uplink. Disables NAPT (so reconnect can
// re-enable it), sets LED to AP_ONLY (red). No-op if gateway never came up.
void patchy_gateway_on_sta_disconnected(void);

// Enable/disable NAPT on the SoftAP netif. No-op if AP not yet up.
esp_err_t patchy_gateway_set_forwarding(bool on);

// Enable/disable the SoftAP itself by toggling Wi-Fi mode between APSTA
// and STA. When disabling, NAPT is dropped first; when enabling, NAPT is
// restored. Fails if the gateway has not been brought up yet.
esp_err_t patchy_gateway_set_ap_enabled(bool on);

// True if gateway has been brought up at least once this boot.
bool patchy_gateway_is_up(void);
