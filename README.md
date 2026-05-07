# esp-patchy-wifi

ESP Wi-Fi NAT gateway with fault injection. Sit it between a flaky-tolerant device and your real AP, then chaos-test the device's reconnection logic — without touching the real router.

## What it does

```
[client device] --(Wi-Fi)--> [ESP SoftAP] --(NAPT)--> [ESP STA] --(Wi-Fi)--> [real AP] --> internet
                                            ^
                                            |
                                     fault injector toggles
                                     NAPT to simulate outages
```

The chip provisions over BLE (NimBLE) via the `espressif/rmaker_app_network` helper — the same flow ESP RainMaker devices use, so any RainMaker-compatible phone app (or the `esp-prov` CLI) can provision it. A QR code is printed to the serial monitor on boot. Once it has Wi-Fi credentials the device joins the uplink AP, brings up its own SoftAP, and forwards traffic via lwIP NAPT. A single button drives three fault-injection modes; an RGB LED reports state.

## Supported targets

ESP32-C6 (default), ESP32-C3, ESP32-S3, ESP32. ESP32-S2 has no BLE — switch the provisioning scheme to SoftAP via menuconfig if targeting S2.

## Build, flash, monitor

```
idf.py set-target esp32c6
idf.py build flash monitor
```

## Provision

On first boot LED is white. The serial monitor prints a QR code and service name (prefixed `PATCHY_…`). Scan the QR code with the ESP RainMaker or ESP BLE Provisioning phone app, or run `esp-prov`:

```
esp-prov --transport ble --service_name PATCHY_XXXXXX \
         --sec_ver 1 --pop patchy1234 \
         --ssid <uplink-ssid> --passphrase <uplink-pw>
```

Default PoP is `patchy1234` (change in menuconfig → *Patchy Wi-Fi Configuration*). Service-name prefix is configurable via menuconfig → *ESP RainMaker App Wi-Fi Provisioning → Provisioning Name Prefix*.

After success, LED turns green. Connect any client to the chip's SoftAP (default SSID `patchy-wifi`, password `patchywifi`) and you have internet.

## Fault injection

| Gesture                  | Effect                                                         | LED cue                                         |
| ------------------------ | -------------------------------------------------------------- | ----------------------------------------------- |
| Single click (<1 s)      | Manual toggle: flip simulated disconnect on/off                | green ↔ blue                                   |
| Hold ≥1 s, release <5 s  | Jittery mode on/off: random outages at random intervals        | **yellow while holding** = release to toggle   |
| Hold ≥5 s                | Network reset: wipe Wi-Fi creds and reboot into provisioning   | yellow → white at the 5 s mark = reset firing  |

Simulated "disconnect" = NAPT disabled on the AP netif. Clients stay associated but packets don't forward upstream. Fast, no STA reconnect penalty.

## LED states

| Color | Meaning                                                                |
| ----- | ---------------------------------------------------------------------- |
| White | Pre-provisioning (no creds, BLE advertising)                           |
| Red   | AP up but uplink STA not connected                                     |
| Green | Connected + forwarding                                                 |
| Blue  | Connected + simulated disconnect                          |

## Console commands

A UART REPL is exposed over the same serial port used for `idf.py monitor`. Prompt is `patchy>`. Type `help` for the auto-generated list. All fault-injection commands require the gateway to be up (i.e. the STA is associated and the SoftAP is running) and return `ESP_ERR_INVALID_STATE` otherwise.

| Command                  | Effect                                                                                                |
| ------------------------ | ----------------------------------------------------------------------------------------------------- |
| `manual [on\|off]`       | Force a simulated disconnect on/off. With no argument, toggles. Refused while a timed mode is active. |
| `jitter <on\|off>`       | Enable/disable jittery mode (random outages bounded by the jitter Kconfig values).                    |
| `cycle <N> <DOWN_S> <UP_S>` | Run `N` cycles of `DOWN_S` seconds down then `UP_S` seconds up, then return to OFF.                |
| `stop`                   | Stop any active fault mode and resume normal forwarding.                                              |
| `status`                 | Print the current fault mode and the active injection mechanism.                                      |
| `inject [napt\|softap]`  | Get or set the injection mechanism (see below). Refused while a fault is currently injected.          |
| `reset --yes`            | Wipe Wi-Fi credentials and reboot into provisioning. Equivalent to the ≥5 s button hold.              |

### Injection mechanisms

The `inject` command picks **how** a simulated outage is produced. Switching modes is only allowed in the OFF state — `stop` first if a fault is currently injected.

| Mode     | Behavior                                                                                                                  |
| -------- | ------------------------------------------------------------------------------------------------------------------------- |
| `napt`   | Default. Toggles NAPT forwarding on the SoftAP netif. Clients stay associated; only upstream packets are blocked.         |
| `softap` | Tears down the SoftAP entirely by switching Wi-Fi mode to STA-only. The patchy SSID disappears; clients are deauthed.     |

## Configuration

```
idf.py menuconfig   # → Patchy Wi-Fi Configuration
```

Adjust SoftAP SSID/password, jitter bounds (uptime/downtime min-max seconds), button GPIO, hold thresholds, and PoP.

## Reset provisioning

Hold the BOOT button for ≥5 seconds. The device wipes stored Wi-Fi credentials, reboots, and returns to the white/provisioning state with BLE advertising active. No need to re-flash.

## Codebase Maintenance

Please use the **provided pre-commit hooks**! Easiest way:

```bash
# Install pre-commit
python3 -m venv .venv
source .venv/bin/activate
pip install pre-commit

# Install the hooks
pre-commit install
```
