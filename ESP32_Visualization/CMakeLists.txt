set(include_dirs
    "."
    "components/inc"
    # Putanja do zajedničkih headera u STM32 mapi
    "../../Sound Localization/Core/CustomDriverSet/IncDriver"
)

idf_component_register(SRCS "main.c" 
                            "components/src/uart_driver.c"
                            "components/src/sound_loc_processor.c"
                            "components/src/wifi_manager.c"
                            "components/src/web_server.c"
                       INCLUDE_DIRS ${include_dirs}
)