#include "stm_pro_mode.h"

static const char *TAG_STM_PRO = "stm_pro_mode";

//Functions for custom adjustments
void initFlashUART(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_driver_install(UART_CONTROLLER, UART_BUF_SIZE * 2, 0, 0, NULL, 0);

    uart_param_config(UART_CONTROLLER, &uart_config);
    uart_set_pin(UART_CONTROLLER, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    logI(TAG_STM_PRO, "%s", "Initialized Flash UART");
}

void initSPIFFS(void)
{
    logI(TAG_STM_PRO, "%s", "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf =
        {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            logE(TAG_STM_PRO, "%s", "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            logE(TAG_STM_PRO, "%s", "Failed to find SPIFFS partition");
        }
        else
        {
            logE(TAG_STM_PRO, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

/*  // Formatting SPIFFS - Use only for debugging
    if (esp_spiffs_format(NULL) != ESP_OK)
    {
        logE(TAG_STM_PRO, "%s", "Failed to format SPIFFS");
        return;
    }
*/
    size_t total, used;
    if (esp_spiffs_info(NULL, &total, &used) == ESP_OK)
    {
        logI(TAG_STM_PRO, "Partition size: total: %d, used: %d", total, used);
    }
}

void initGPIO(void)
{
    gpio_set_direction(RESET_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RESET_PIN, HIGH);
    gpio_set_direction(BOOT0_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BOOT0_PIN, HIGH);

    logI(TAG_STM_PRO, "%s", "GPIO Initialized");
}

void resetSTM(void)
{
    logI(TAG_STM_PRO, "%s", "Starting RESET Procedure");

    gpio_set_level(RESET_PIN, LOW);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    gpio_set_level(RESET_PIN, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    logI(TAG_STM_PRO, "%s", "Finished RESET Procedure");
}

void endConn(void)
{
    gpio_set_level(RESET_PIN, LOW);
    gpio_set_level(BOOT0_PIN, LOW);

    resetSTM();

    logI(TAG_STM_PRO, "%s", "Ending Connection");
}

void setupSTM(void)
{
    resetSTM();
    cmdSync();
    cmdGet();
    logI(TAG_STM_PRO, "%s", "Ending GET");
    cmdVersion();
    logI(TAG_STM_PRO, "%s", "Ending Version");
    cmdId();
    logI(TAG_STM_PRO, "%s", "Ending ID");
    // cmdErase();
    cmdExtErase();
}

int cmdSync(void)
{
    logI(TAG_STM_PRO, "%s", "SYNC");

    char bytes[] = {0x7F};
    int resp = 1;
    return sendBytes(bytes, sizeof(bytes), resp, 1000);
}

int cmdGet(void)
{
    logI(TAG_STM_PRO, "%s", "GET");

    char bytes[] = {0x00, 0xFF};
    int resp = 15;
    return sendBytes(bytes, sizeof(bytes), resp, 1000);
}

int cmdVersion(void)
{
    logI(TAG_STM_PRO, "%s", "GET VERSION & READ PROTECTION STATUS");

    char bytes[] = {0x01, 0xFE};
    int resp = 5;
    return sendBytes(bytes, sizeof(bytes), resp, 1000);
}

int cmdId(void)
{
    logI(TAG_STM_PRO, "%s", "CHECK ID");
    char bytes[] = {0x02, 0xFD};
    int resp = 5;
    return sendBytes(bytes, sizeof(bytes), resp, 1000);
}

int cmdErase(void)
{
    logI(TAG_STM_PRO, "%s", "ERASE MEMORY");
    char bytes[] = {0x43, 0xBC};
    int resp = 1;
    int a = sendBytes(bytes, sizeof(bytes), resp, 1000);

    if (a == 1)
    {
        char params[] = {0xFF, 0x00};
        resp = 1;

        return sendBytes(params, sizeof(params), resp, 1000);
  
    }
    return 0;
}

int cmdExtErase(void)
{
    logI(TAG_STM_PRO, "%s", "EXTENDED ERASE MEMORY");
    char bytes[] = {0x44, 0xBB};
    int resp = 1;
    int a = sendBytes(bytes, sizeof(bytes), resp, 1000);
    logI(TAG_STM_PRO, "a: %d", a);
    if (a == 1)
    {
        char params[] = {0xFF, 0xFF, 0x00};
        resp = 1;

        return sendBytes(params, sizeof(params), resp, 10000);
    }
    return 0;
}

int cmdWrite(void)
{
    logI(TAG_STM_PRO, "%s", "WRITE MEMORY");
    char bytes[2] = {0x31, 0xCE};
    int resp = 1;
    return sendBytes(bytes, sizeof(bytes), resp, 1000);
}

int cmdRead(void)
{
    logI(TAG_STM_PRO, "%s", "READ MEMORY");
    char bytes[2] = {0x11, 0xEE};
    int resp = 1;
    return sendBytes(bytes, sizeof(bytes), resp, 1000);
}

int loadAddress(const char adrMS, const char adrMI, const char adrLI, const char adrLS)
{
    char xor = adrMS ^ adrMI ^ adrLI ^ adrLS;
    char params[] = {adrMS, adrMI, adrLI, adrLS, xor};
    int resp = 1;

    // ESP_LOG_BUFFER_HEXDUMP("LOAD ADDR", params, sizeof(params), ESP_LOG_DEBUG);
    return sendBytes(params, sizeof(params), resp, 1000);
}

int sendBytes(const char *bytes, int count, int resp, int ticks_to_wait)
{
    uart_flush_input(UART_CONTROLLER);
    for (int i = 0; i < count; i++)
    {
        sendData(TAG_STM_PRO, &bytes[i], 1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    int length;
    int ret_val = 0;
    int max_retry = 5;
    while (max_retry > 0){
        length = waitForSerialData(resp, SERIAL_TIMEOUT); // resp = 1, 
        if (length > 0)
        {   
            logI(TAG_STM_PRO, "LENGTH: %d", length);
            uint8_t data[length];
            const int rxBytes = uart_read_bytes(UART_CONTROLLER, data, length, ticks_to_wait / portTICK_PERIOD_MS);
            ESP_LOG_BUFFER_HEX(TAG_STM_PRO, data, rxBytes);
            if (rxBytes > 0 && data[0] == ACK)
            {
                logI(TAG_STM_PRO, "%s", "Sync Success");
                ESP_LOG_BUFFER_HEXDUMP("SYNC", data, rxBytes, ESP_LOG_DEBUG);
                ret_val = 1;
                break;
            }
            else
            {
                logE(TAG_STM_PRO, "%s", "Sync Failure");
                ret_val = 0;
                break;
            }
        }
        else
        {
            logE(TAG_STM_PRO, "%s", "Serial Timeout");
            ret_val = 0;
        }
        max_retry--;
    }
    return ret_val;
}

int sendData(const char *logName, const char *data, const int count)
{
    const int txBytes = uart_write_bytes(UART_CONTROLLER, data, count);
    //logD(logName, "Wrote %d bytes", count);
    return txBytes;
}

int waitForSerialData(int dataCount, int timeout)
{
    int timer = 0;
    int length = 0;
    const TickType_t delay = timeout / portTICK_PERIOD_MS;
    while (timer < delay)
    {
        uart_get_buffered_data_len(UART_CONTROLLER, (size_t *)&length);
        if (length >= dataCount)
        {
            return length;
        }
        vTaskDelay(portTICK_PERIOD_MS);
        timer++;
    }
    return 0;
}

void incrementLoadAddress(char *loadAddr)
{
    loadAddr[2] += 0x1;

    if (loadAddr[2] == 0)
    {
        loadAddr[1] += 0x1;

        if (loadAddr[1] == 0)
        {
            loadAddr[0] += 0x1;
        }
    }
}

esp_err_t flashPage(const char *address, const char *data)
{
    uart_flush(UART_CONTROLLER);
    logI(TAG_STM_PRO, "%s", "Flashing Page");

    cmdWrite();

    loadAddress(address[0], address[1], address[2], address[3]);

    //ESP_LOG_BUFFER_HEXDUMP("FLASH PAGE", data, 256, ESP_LOG_DEBUG);
    logI(TAG_STM_PRO, "Flashing address: %02X %02X %02X %02X", address[0], address[1], address[2], address[3]);

    char xor = 0xFF;
    char sz = 0xFF;

    sendData(TAG_STM_PRO, &sz, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    logI(TAG_STM_PRO, "Flashing size: %d", sz + 1);
    for (int i = 0; i < 256; i++)
    {
        sendData(TAG_STM_PRO, &data[i], 1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        xor ^= data[i];
    }
    logI(TAG_STM_PRO, "Flashing XOR: %02X", xor);
    sendData(TAG_STM_PRO, &xor, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    int length;
    int ret_val = ESP_FAIL;
    int max_retry = 5;
    while(max_retry > 0){
        length = waitForSerialData(1, SERIAL_TIMEOUT);
        if (length > 0)
        {
            uint8_t data[length];
            const int rxBytes = uart_read_bytes(UART_CONTROLLER, data, length, 1000 / portTICK_PERIOD_MS);
            if (rxBytes > 0 && data[0] == ACK)
            {
                logI(TAG_STM_PRO, "%s", "Flash Success");
                ret_val = ESP_OK;
                break;
            }
            else
            {
                logE(TAG_STM_PRO, "%s", "Flash Failure");
                ret_val = ESP_FAIL;
                break;
            }
        }
        else
        {
            logE(TAG_STM_PRO, "%s", "Serial Timeout");
        }
        max_retry--;
    }
    return ret_val;
}

esp_err_t readPage(const char *address, const char *data)
{
    uart_flush(UART_CONTROLLER);
    logI(TAG_STM_PRO, "%s", "Reading page");
    char param[] = {0xFF, 0x00};

    cmdRead();

    loadAddress(address[0], address[1], address[2], address[3]);
    logI(TAG_STM_PRO, "Reading address: %02X %02X %02X %02X", address[0], address[1], address[2], address[3]);
    
    sendData(TAG_STM_PRO, &param[0], sizeof(param[0]));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    sendData(TAG_STM_PRO, &param[1], sizeof(param[1]));

    logI(TAG_STM_PRO, "Reading size: %d", param[0] + 1);
    int max_retry = 5;
    int length;
    int ret_val = ESP_OK;
    while(max_retry > 0){
        length = waitForSerialData(257, SERIAL_TIMEOUT);
        if (length > 0)
        {
            uint8_t uart_data[length];
            const int rxBytes = uart_read_bytes(UART_CONTROLLER, uart_data, length, 1000 / portTICK_PERIOD_MS);

            if (rxBytes > 0 && uart_data[0] == 0x79)
            {
                logI(TAG_STM_PRO, "%s", "Success");
                memcpy((void *)data, uart_data, 257);
                break;
                //ESP_LOG_BUFFER_HEXDUMP("READ MEMORY", data, rxBytes, ESP_LOG_DEBUG);
            }
            else
            {
                logE(TAG_STM_PRO, "%s", "Failure");
                ret_val = ESP_FAIL;
                break;
            }
        }
        else
        {
            logE(TAG_STM_PRO, "%s", "Serial Timeout");
            ret_val = ESP_FAIL;
        }
        max_retry--;
    }
    return ret_val;
}