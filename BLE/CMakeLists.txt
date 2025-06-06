idf_component_register(
    SRCS
        "main.c"
        "ble_server.c"
    INCLUDE_DIRS
        "."
    REQUIRES          # nagłówki + biblioteki z tych komponentów
        nvs_flash
        bt                 # NimBLE i esp_bt.h
)
