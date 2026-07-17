# STM32 OTA Update using ESP32

A comprehensive firmware Over-The-Air (OTA) update system that enables ESP32 to download and flash firmware updates to STM32 microcontrollers via MQTT and HTTP protocols.

## Overview

This project implements a robust OTA update mechanism where an ESP32 device acts as an intermediary to receive OTA commands via MQTT and manage the download and flashing of firmware updates to an STM32 device. The system includes:

- **WiFi Connectivity**: Automatic WiFi connection with retry logic
- **MQTT Communication**: Subscribes to OTA commands and publishes status updates
- **HTTP Firmware Download**: Secure download of firmware binaries via HTTPS
- **STM32 Flashing**: Direct firmware programming to STM32 via UART
- **Web Server**: File server interface for monitoring and management
- **Event-Driven Architecture**: FreeRTOS-based task scheduling and event handling

## Features

- ✅ MQTT-based OTA command reception
- ✅ Secure HTTPS firmware downloads with certificate validation
- ✅ Status reporting (PING, DOWNLOADING, FLASHING)
- ✅ WiFi auto-reconnection with configurable retries
- ✅ SPIFFS file storage for intermediate firmware binary storage
- ✅ Custom partition configuration for OTA updates
- ✅ JSON-based command and response protocol
- ✅ Comprehensive logging system
- ✅ GPIO initialization for control signals
- ✅ Web interface for file management

## Hardware Requirements

- **ESP32 Development Board** (Primary device)
- **STM32 Microcontroller** (Target device for OTA updates)
- **Serial Connection** (UART for STM32 programming)
- **WiFi Network Access**
- **MQTT Broker Access**

## Project Structure

```
STM32-OTA-using-ESP32/
├── main/                           # Main application
│   ├── main.c                      # Core firmware OTA logic
│   ├── main.h                      # Main header files
│   ├── file_server.c               # Web server implementation
│   ├── CMakeLists.txt              # Main component configuration
│   ├── upload_script.html          # Web UI for file uploads
│   └── favicon.ico                 # Web server icon
├── components/
│   ├── logger/                     # Logging component
│   │   ├── logger.c
│   │   ├── include/logger.h
│   │   └── CMakeLists.txt
│   └── stm_flash/                  # STM32 flashing component
│       ├── stm_flash.c
│       ├── include/
│       └── CMakeLists.txt
├── CMakeLists.txt                  # Project-level CMake config
├── Makefile                        # Build configuration
├── partitions_example.csv          # Custom partition table
├── sdkconfig                       # Full SDK configuration
├── sdkconfig.defaults              # Default SDK settings
├── LICENSE                         # MIT License
└── .clangd                         # Clang configuration

```

## Configuration

### WiFi Credentials

Edit `main/main.c` to configure your WiFi settings:

```c
#define EXAMPLE_ESP_WIFI_SSID      "YOUR_SSID"
#define EXAMPLE_ESP_WIFI_PASS      "YOUR_PASSWORD"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
```

### MQTT Configuration

Update MQTT broker details in `main/main.c`:

```c
#define MQTT_HOST                   "mqtt-broker.example.com"
#define MQTT_USERNAME               "username"
#define MQTT_PASSWORD               "password"
#define MQTT_PORT                   1883
```

The device uses its MAC address as the topic identifier:
- **Subscribe Topic**: `{MAC_ADDRESS}/ota`
- **Publish Topic**: `{MAC_ADDRESS}/ota/ack`

### Firmware Version

Update the firmware version string:

```c
static char* firmware_version = "v0.0.0";
```

## Build and Flash

### Prerequisites

- ESP-IDF (ESP32 IoT Development Framework)
- CMake 3.5+
- Python 3.7+

### Building

```bash
# Set ESP-IDF environment
source ~/esp/esp-idf/export.sh

# Build the project
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

### Configuration Menuconfig

```bash
idf.py menuconfig
```

Key configurations:
- Serial flasher config (PORT, BAUD RATE)
- WiFi configuration
- MQTT settings
- Partition table settings

## MQTT Protocol

### OTA Command Format

Subscribe to receive OTA commands as JSON:

```json
{
  "type": "OTA_COMMAND",
  "path": "https://example.com/firmware/v1.0.0.bin",
  "version": "v1.0.0"
}
```

### PING Command

Health check command:

```json
{
  "type": "PING_COMMAND"
}
```

### Response Format

Device publishes status updates:

```json
{
  "status": "ONLINE|DOWNLOADING|FLASHING|COMPLETED|ERROR",
  "version": "v1.0.0"
}
```

## Workflow

```
┌─────────────────────────────────────────────────────────┐
│  MQTT Broker sends OTA_COMMAND with firmware URL        │
└────────────────┬────────────────────────────────────────┘
                 │
                 ▼
         ┌───────────────┐
         │  ESP32 Receives│
         │  OTA Command  │
         └───────┬───────┘
                 │
                 ▼
      ┌──────────────────────┐
      │ Download firmware via │
      │ HTTPS (HTTP handler) │
      │ Save to SPIFFS       │
      └──────────┬───────────┘
                 │
                 ▼
      ┌──────────────────────┐
      │ Create Flash Task    │
      │ Read binary from     │
      │ SPIFFS               │
      └──────────┬───────────┘
                 │
                 ▼
      ┌──────────────────────┐
      │ Program STM32 via    │
      │ UART                 │
      └──────────┬───────────┘
                 │
                 ▼
      ┌──────────────────────┐
      │ Publish status ACK   │
      │ to MQTT broker       │
      └──────────────────────┘
```

## Key Components

### Main Application (`main/main.c`)

- **WiFi Initialization**: Connects ESP32 to WiFi with automatic reconnection
- **MQTT Event Handler**: Processes incoming OTA and PING commands
- **HTTP Download Handler**: Manages firmware binary downloads via HTTPS
- **Flash Task**: Creates FreeRTOS task to write firmware to STM32

### SPIFFS Storage

The system uses SPIFFS (SPI Flash File System) for temporary storage:
- Downloaded firmware is stored at `/spiffs/hehe.bin`
- Provides non-volatile storage between power cycles

### Event Groups

- `s_wifi_event_group`: Tracks WiFi connection state
- `s_global_event_group`: Manages global application events

### Custom Partition Table

The project uses a custom partition table (`partitions_example.csv`) configured for:
- 4MB Flash memory
- OTA partition management
- Custom partition sizing

## Dependencies

Key ESP-IDF components used:

- `esp_wifi` - WiFi functionality
- `esp_mqtt_client` - MQTT client
- `esp_http_client` - HTTP/HTTPS client
- `json` (cJSON) - JSON parsing
- `nvs_flash` - Non-volatile storage
- `spiffs` - File system
- `freertos` - Real-time operating system

## Logging

The project includes a custom logging component with `logI()` and `logE()` functions for debug output. Serial logs provide detailed information about:
- WiFi connection status
- MQTT events
- HTTP download progress
- Firmware flashing operations

## Security Considerations

- ✅ HTTPS with certificate validation for firmware downloads
- ⚠️ **TODO**: Review hardcoded WiFi and MQTT credentials - consider using secure storage (NVS encryption)
- ⚠️ **TODO**: Implement firmware signature verification before flashing
- ⚠️ **TODO**: Add MQTT TLS/SSL support

## Troubleshooting

### WiFi Connection Issues

- Verify SSID and password in `main.c`
- Check WiFi signal strength
- Monitor serial output for connection events

### MQTT Connection Failures

- Verify MQTT broker address and port
- Check username and password credentials
- Ensure device can reach broker (firewall/network issues)

### Firmware Download Errors

- Verify URL is accessible and returns HTTP 200
- Check ESP32 storage space (SPIFFS)
- Monitor HTTP event logs

### STM32 Flashing Issues

- Verify UART connection between ESP32 and STM32
- Check baud rate configuration
- Ensure STM32 is in bootloader mode if required

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Author

**NguyenBKerK22**

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Future Enhancements

- [ ] Add firmware signature verification (RSA/ECDSA)
- [ ] Implement rollback on failed flash
- [ ] Add progress reporting via MQTT
- [ ] Support multiple STM32 target configurations
- [ ] Web dashboard for OTA management
- [ ] Encrypted credential storage using NVS encryption
- [ ] MQTT TLS/SSL support
- [ ] Delta firmware updates for reduced download size

---

**Note**: This project contains hardcoded credentials for development purposes. In production, use secure credential storage mechanisms like NVS encryption or AWS IoT certificates.
