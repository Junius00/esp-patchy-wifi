/*
 * SPDX-FileCopyrightText: 2026 Junius Pun
 *
 * SPDX-License-Identifier: MIT
 */

#include "console.h"
#include "fault_injector.h"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "patchy-cli";

static const char *mode_str(patchy_fault_mode_t m)
{
    switch (m) {
    case FAULT_OFF:         return "OFF";
    case FAULT_MANUAL_DOWN: return "MANUAL_DOWN";
    case FAULT_JITTERY:     return "JITTERY";
    case FAULT_CYCLICAL:    return "CYCLICAL";
    }
    return "?";
}

static int print_err(esp_err_t err, const char *cmd)
{
    if (err == ESP_OK) {
        printf("%s: ok\n", cmd);
        return 0;
    }
    printf("%s: %s\n", cmd, esp_err_to_name(err));
    return 1;
}

// ---- manual ----
static int cmd_manual(int argc, char **argv)
{
    bool down;
    if (argc < 2) {
        // no arg → toggle
        down = (patchy_fault_get_mode() != FAULT_MANUAL_DOWN);
    } else if (strcasecmp(argv[1], "on") == 0) {
        down = true;
    } else if (strcasecmp(argv[1], "off") == 0) {
        down = false;
    } else {
        printf("manual: expected 'on' or 'off', got '%s'\n", argv[1]);
        return 1;
    }
    return print_err(patchy_fault_manual_set(down), "manual");
}

// ---- jitter ----
static int cmd_jitter(int argc, char **argv)
{
    if (argc < 2) {
        printf("jitter: expected 'on' or 'off'\n");
        return 1;
    }
    bool enable;
    if (strcasecmp(argv[1], "on") == 0) {
        enable = true;
    } else if (strcasecmp(argv[1], "off") == 0) {
        enable = false;
    } else {
        printf("jitter: expected 'on' or 'off', got '%s'\n", argv[1]);
        return 1;
    }
    return print_err(patchy_fault_jittery_set(enable), "jitter");
}

// ---- cycle ----
static struct {
    struct arg_int *n;
    struct arg_int *down_s;
    struct arg_int *up_s;
    struct arg_end *end;
} s_cycle_args;

static int cmd_cycle(int argc, char **argv)
{
    int nerr = arg_parse(argc, argv, (void **)&s_cycle_args);
    if (nerr != 0) {
        arg_print_errors(stderr, s_cycle_args.end, argv[0]);
        return 1;
    }
    int n = s_cycle_args.n->ival[0];
    int d = s_cycle_args.down_s->ival[0];
    int u = s_cycle_args.up_s->ival[0];
    if (n < 1 || d < 1 || u < 1) {
        printf("cycle: N, DOWN_S, UP_S must all be >= 1\n");
        return 1;
    }
    return print_err(patchy_fault_cycle_start((uint32_t)n, (uint32_t)d, (uint32_t)u),
                     "cycle");
}

// ---- stop ----
static int cmd_stop(int argc, char **argv)
{
    return print_err(patchy_fault_stop(), "stop");
}

// ---- status ----
static int cmd_status(int argc, char **argv)
{
    printf("mode: %s\n", mode_str(patchy_fault_get_mode()));
    return 0;
}

// ---- reset ----
static struct {
    struct arg_lit *yes;
    struct arg_end *end;
} s_reset_args;

static int cmd_reset(int argc, char **argv)
{
    int nerr = arg_parse(argc, argv, (void **)&s_reset_args);
    if (nerr != 0) {
        arg_print_errors(stderr, s_reset_args.end, argv[0]);
        printf("reset: pass --yes to confirm wiping Wi-Fi credentials and rebooting\n");
        return 1;
    }
    printf("reset: wiping creds and rebooting...\n");
    fflush(stdout);
    patchy_fault_reset();
    return 0;   // unreachable, esp_restart fires
}

static esp_err_t register_commands(void)
{
    const esp_console_cmd_t manual_cmd = {
        .command = "manual",
        .help = "Manual disconnect toggle (no arg) or explicit 'on'/'off'",
        .hint = "[on|off]",
        .func = &cmd_manual,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&manual_cmd));

    const esp_console_cmd_t jitter_cmd = {
        .command = "jitter",
        .help = "Enable/disable jittery mode (random outages from Kconfig bounds)",
        .hint = "<on|off>",
        .func = &cmd_jitter,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&jitter_cmd));

    s_cycle_args.n      = arg_int1(NULL, NULL, "<N>",       "number of cycles");
    s_cycle_args.down_s = arg_int1(NULL, NULL, "<DOWN_S>",  "downtime seconds per cycle");
    s_cycle_args.up_s   = arg_int1(NULL, NULL, "<UP_S>",    "uptime seconds per cycle");
    s_cycle_args.end    = arg_end(4);
    const esp_console_cmd_t cycle_cmd = {
        .command = "cycle",
        .help = "Run N cycles of (down DOWN_S then up UP_S), then return to OFF",
        .hint = NULL,
        .func = &cmd_cycle,
        .argtable = &s_cycle_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cycle_cmd));

    const esp_console_cmd_t stop_cmd = {
        .command = "stop",
        .help = "Stop any active fault mode and resume forwarding",
        .hint = NULL,
        .func = &cmd_stop,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&stop_cmd));

    const esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Print current fault mode",
        .hint = NULL,
        .func = &cmd_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));

    s_reset_args.yes = arg_lit1(NULL, "yes", "confirm wiping Wi-Fi credentials");
    s_reset_args.end = arg_end(2);
    const esp_console_cmd_t reset_cmd = {
        .command = "reset",
        .help = "Wipe Wi-Fi credentials and reboot into provisioning (requires --yes)",
        .hint = NULL,
        .func = &cmd_reset,
        .argtable = &s_reset_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reset_cmd));

    return ESP_OK;
}

esp_err_t patchy_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "patchy> ";
    repl_config.max_cmdline_length = 128;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_register_help_command());
    ESP_ERROR_CHECK(register_commands());
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    ESP_LOGI(TAG, "console ready — type 'help' for commands");
    return ESP_OK;
}
