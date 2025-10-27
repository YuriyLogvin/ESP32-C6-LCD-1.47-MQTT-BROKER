idf_component_register(SRCS "SpiMipiLvglDisplayDriver.cpp" "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES log PicoMQTT nvs_flash)