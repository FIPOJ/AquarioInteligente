idf_component_register(SRCS "HD44780.c" "ds18b20.c" "hello_world_main.c"
                    INCLUDE_DIRS ""
                    REQUIRES esp_wifi nvs_flash driver esp_http_server esp_http_client)
