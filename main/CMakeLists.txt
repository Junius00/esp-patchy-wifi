idf_component_register(
    SRCS "main.c" "ui.c" "provisioning.c" "gateway.c" "fault_injector.c" "console.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash
             esp_wifi
             esp_netif
             esp_event
             esp_timer
             lwip
             driver
             console
)
