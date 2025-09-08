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
void setupDS18B20();
void setupBME680();
void checkIaqSensorStatus(Bsec2 bsec);
void errLeds();
void bme680Callback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec);

void setupRouting();
void createJson(const char *tag, float value, const char *unit);
void addJsonObject(const char *tag, String value, const char *unit);
void readDS18B20SensorData(void * parameter);
void readBME680SensorData(void * parameter);

void getSimpleReadings();
void getAllReadings();
void setupSensorDataPoller();
