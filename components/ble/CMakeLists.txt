idf_component_register(
    SRCS
        "ble_server.c"
        # "ble_server.c"
    INCLUDE_DIRS
        "."
    REQUIRES          # nagłówki + biblioteki z tych komponentów
        nvs_flash
        bt                 # NimBLE i esp_bt.h
)
