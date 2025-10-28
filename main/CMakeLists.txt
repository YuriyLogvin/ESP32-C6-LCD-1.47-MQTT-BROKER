idf_component_register(SRCS "NvsSettingsAccessor.cpp" "SpiMipiLvglDisplayDriver.cpp" "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES log PicoMQTT nvs_flash)