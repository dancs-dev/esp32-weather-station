/*
 WiFi Web Server Weather Station header file.

 A simple web server that displays readings from the weather
 station sensors.

 These currently include the:
 BME 680 (indoor temperature, humidity, pressure, air quality).

 This example is written for a network using WPA encryption. For
 WEP or WPA, change the Wifi.begin() call accordingly.

 Circuit:
 * BME 680 SDA -> D21, SCL -> D22
 
 */

void setupWiFi();
void setupBME680();
void checkIaqSensorStatus();
void errLeds();

void setupRouting();
void createJson(char *tag, float value, char *unit);
void addJsonObject(char *tag, float value, char *unit);
void readSensorData(void * parameter);

void getSimpleReadings();
void getAllReadings();
void setupSensorDataPoller();
