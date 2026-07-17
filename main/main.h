#include "stm_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "protocol_examples_common.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"


void flash_firmware(void *pvParameters);
void fetch_file(void *pvParameters);
void wifi_init_sta(void);

static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void mqtt_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data);

