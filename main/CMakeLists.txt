idf_component_register(
    SRCS
        "main.c"
        # "ble_server.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        display
        ble
        encoder
        uart
        hall_sensor
        timer_utils
        bike_metrics
        tasks
        # display_task
        # encoder_task
        # hall_task
)
