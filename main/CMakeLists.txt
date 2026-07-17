idf_component_register(SRCS "main.c" "file_server.c"
                    INCLUDE_DIRS "."
                    REQUIRES json stm_flash protocol_examples_common mqtt esp_http_client
                    EMBED_FILES "favicon.ico" "upload_script.html")