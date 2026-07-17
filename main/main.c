#include "main.h"

#define EXAMPLE_ESP_WIFI_SSID      "BKIT_NEW"
#define EXAMPLE_ESP_WIFI_PASS      "cselabc5c6"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define MAC_ADDR_SIZE 6
#define MAC_ADDRESS "d4:e9:f4:a1:11:28"

#define MQTT_HOST                   "mqtt-vp3.abcsolutions.com.vn"
#define MQTT_USERNAME               "abc_ots"
#define MQTT_PASSWORD               "AbcOts@2605"
#define MQTT_TOPIC_SUB              MAC_ADDRESS"/ota"
#define MQTT_TOPIC_PUB              MAC_ADDRESS"/ota/ack"
#define MQTT_PORT                   1883



static char* firmware_version = "v0.0.0";
static const char *TAG = "wifi station";
static int s_retry_num = 0;

static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t s_global_event_group;

esp_err_t start_file_server(const char *base_path);

ESP_EVENT_DECLARE_BASE(MQTT_EVENTS);

FILE *fd = NULL; // File descriptor for the file being downloaded

void flash_firmware(void *pvParameters)
{
    char *path = (char *)pvParameters;
    ESP_LOGI(TAG, "Flashing firmware from %s", path);
    FILE *flash_file = fopen("/spiffs/hehe.bin", "rb");
    if (flash_file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for flashing");
        vTaskDelete(NULL);
        return;
    }
    writeTask(flash_file);
    fclose(flash_file);
    endConn();
    vTaskDelete(NULL);
}
void fetch_file(void *pvParameters)
{
    ESP_LOGI(TAG, "Fetching file from server...");
    char path[128];
    char *url = (char *)pvParameters;
    const char *filename = strrchr(url, '/');
    if (filename != NULL)
    {
        filename++;     // bỏ qua ký tự '/'
    }
    snprintf(path, sizeof(path), "/spiffs/%s", filename);
    ESP_LOGI(TAG, "File will be saved to: %s", path);
    esp_http_client_config_t config = {
        .url = "https://ots.quantraconline.com/Uploads/Employees/Firmware/DeviceType_1/test_ota_v0.0.0_20260716_105215.bin",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler     = http_event_handler
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    
    fd = fopen("/spiffs/hehe.bin", "wb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        esp_http_client_cleanup(http_client);
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = esp_http_client_perform(http_client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(http_client);
        if(status == 200) {

            ESP_LOGI(TAG, "File fetched successfully");
            
            ESP_LOGI(TAG, "File saved to %s", path);
            ESP_LOGI(TAG, "Starting firmware update...");
            fclose(fd);
            xTaskCreate(&flash_firmware, "Flash firmware", 4096, (void *)path, 5, NULL);
        } else {
            ESP_LOGE(TAG, "HTTP GET request failed with status code: %d", status);
        }
    } 
    else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(http_client);
    vTaskDelete(NULL);
}
static void wifi_ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    logI(TAG, "Event dispatched from event loop base=%s, event_id=%d", event_base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    if(event_base == MQTT_EVENTS){
        if(event_id == MQTT_EVENT_CONNECTED){
            esp_mqtt_client_subscribe((esp_mqtt_client_handle_t)handler_args, MQTT_TOPIC_SUB, 0);
        }
        else if(event_id == MQTT_EVENT_DISCONNECTED){
            
        }
        else if(event_id == MQTT_EVENT_DATA){
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);

            printf("DATA=%.*s\r\n", event->data_len, event->data);
            printf("DATA_LEN=%d\r\n", event->data_len);

            cJSON *root = cJSON_Parse(event->data);
            cJSON *item = cJSON_GetObjectItem(root, "type");

            if(strcmp(item->valuestring, "PING_COMMAND") == 0){
                ESP_LOGI(TAG, "PING_COMMAND received");

                cJSON *ack_payload = cJSON_CreateObject();
                // Add key-value pairs
                cJSON_AddStringToObject(ack_payload, "status", "ONLINE");

                esp_mqtt_client_publish(client, MQTT_TOPIC_PUB, cJSON_Print(ack_payload), 0, 1, 0);

                cJSON_Delete(ack_payload);
            }
            else if(strcmp(item->valuestring, "OTA_COMMAND") == 0){
                ESP_LOGI(TAG, "OTA_COMMAND received");
                cJSON *ota_item = cJSON_GetObjectItem(root, "path");
                cJSON *version_item = cJSON_GetObjectItem(root, "version");

                firmware_version = version_item->valuestring;

                cJSON *ack_payload = cJSON_CreateObject();
                // Add key-value pairs
                cJSON_AddStringToObject(ack_payload, "status", "DOWNLOADING");
                cJSON_AddStringToObject(ack_payload, "version", firmware_version);

                esp_mqtt_client_publish(client, MQTT_TOPIC_PUB, cJSON_Print(ack_payload), 0, 1, 0);

                xTaskCreate(&fetch_file, "Fetch file from server", 4096, (void *)ota_item->valuestring, 5, NULL);

                cJSON_Delete(ack_payload);
            }
            cJSON_Delete(root);
        }
        else if(event_id == MQTT_EVENT_PUBLISHED){
            logI(TAG, "Event dispatched from event loop base=%s, event_id=%d", event_base, event_id);
        }
    }
}
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        ESP_LOGI(TAG, "Data: %.*s", evt->data_len, (char *)evt->data);
        if (fd != NULL) {
            ESP_LOGI(TAG, "Writing data to file");
           fwrite(evt->data, 1, evt->data_len, fd);
        }
    }
    return ESP_OK;
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        
                                                        &wifi_ip_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_ip_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}
void app_main(void)
{      
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    /* Initialize file storage */
    initSPIFFS();

    uint8_t mac[MAC_ADDR_SIZE];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    ESP_LOGI("MAC address", "MAC address: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* Start the file server */
    // ESP_ERROR_CHECK(start_file_server("/spiffs"));
    
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = "mqtt://"MQTT_HOST,
            },
        },
        .credentials.username = MQTT_USERNAME, // Optional: Set username
        .credentials.authentication.password = MQTT_PASSWORD // Optional: Set password
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    initGPIO();
    initFlashUART();

    ESP_ERROR_CHECK(start_file_server("/spiffs"));
}
