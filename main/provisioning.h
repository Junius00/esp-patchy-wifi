/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

// Brings up NVS, netif, default event loop, Wi-Fi (STA), and — if not yet
// provisioned — runs the network_provisioning manager over BLE until creds
// are accepted. Returns after the STA has been told to connect (does not
// wait for IP).
esp_err_t patchy_prov_start(void);

// True once network_prov_mgr_is_provisioned() returned true.
bool patchy_prov_is_provisioned(void);

// Wipe stored Wi-Fi credentials so next boot re-enters provisioning.
esp_err_t patchy_prov_reset(void);
