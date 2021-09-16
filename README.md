# ESP32 Weather Station + API

## Installation

1. Download and install Visual Studio Code.
1. Install the PlatformIO extension for Visual Studio Code.
1. Open this project. You can build and upload to you ESP32 accordingly.
1. To upload to the ESP32, when `connecting...` is output from the upload
   process, you may need to press and hold (for approx. 2 seconds) the upload
   button (may be indicated as `BOOT`).

## Setup

1. You must add in your WiFi details to `ssid` and `password` in `main.cpp`.
1. You can find the IP address your ESP32 was assigned by opening the Serial
   Monitor and reseting your ESP32 (may be the `EN` button).

## Circuit

Circuit diagram will be added here when project is further developed.

## Sensors

The following sensors are currently used:
- `BME680`: Intended for indoor temperature, humidity, pressure, and iaq.

More will be added as project matures.

## Endpoints available

The following endpoints are currently available:
- `/all` - all sensor data that is available.
- `/simple` - the most useful sensor data that is available.
