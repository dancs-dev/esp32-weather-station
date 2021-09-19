# ESP32 Weather Station + API

## Installation

1. Download and install Visual Studio Code.
1. Install the PlatformIO extension for Visual Studio Code.
1. Open this project. You can build and upload to you ESP32 accordingly.
1. To upload to the ESP32, when `connecting...` is output from the upload
   process, you may need to press and hold (for approx. 2 seconds) the upload
   button (may be indicated as `BOOT`).

## Setup

1. You must create a file called `settings.h` in `/include`.
1. You must create the following definitions, filling in your details:
    - `#define SSID "ssid"`
    - `#define PASSWORD "password"`
    - `#define PORT 80`
1. You can find the IP address your ESP32 was assigned by opening the Serial
   Monitor and reseting your ESP32 (may be the `EN` button).
1. To ensure your weather station starts on the same local IP address each time,
   you can add a rule to the DHCP server of your router for the particular MAC
   address and desired IP address.
1. There are numerous configuration options in `main.cpp`, including reading
   offsets and pin numbers. The values there are the values that worked best for
   me.

Note: the BME680 sensor [must be read every 3 seconds +/- 50%](
   https://community.bosch-sensortec.com/t5/MEMS-sensors-forum/Bme680-bsec-iaq-accuracy-is-always-0/m-p/22532/highlight/true#M6545).
   Therefore the polling frequency should probably remain quite low.

## Circuit

Circuit diagram will be added here when project is further developed.

## Sensors

The following sensors are currently used:
- `BME680`: Intended for indoor temperature, humidity, pressure, and iaq.
- `DS18B20`: Waterproof probe version intended for outdoor temperature.

More will be added as project matures.

## Endpoints available

The following endpoints are currently available:
- `/all` - all sensor data that is available.
- `/simple` - the most useful sensor data that is available.
