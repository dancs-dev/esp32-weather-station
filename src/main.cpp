/*
 WiFi Web Server Weather Station

 A simple web server that displays readings from the weather
 station sensors.

 These currently include the:
 BME 680 (indoor temperature, humidity, pressure, air quality).

 This example is written for a network using WPA encryption. For
 WEP or WPA, change the Wifi.begin() call accordingly.

 Circuit:
 * BME 680 SDA -> D21, SCL -> D22
 
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <bsec.h>

#include "main.h"

#define POLLING_FREQUENCY 30

const char* ssid     = "ssid";
const char* password = "password";

WebServer server(80);

Bsec iaqSensor;

StaticJsonDocument<2500> jsonDocument;
char buffer[2500];

String output;

float rawTempO, pressureO, rawHumidityO, gasO, iaqO, staticIaqO, co2eO, breathVoceO, calTempO, calHum0;
int iaqAccuracyO;

void setup() {
    Serial.begin(115200);
    
    setupWiFi();
    setupBME680();

    setupSensorDataPoller();
    setupRouting();
}

void loop(){
    server.handleClient();
}

void setupWiFi(void) {
    // Connect to WiFi

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setupBME680(void) {
    // Setup the BME 680
    
    Wire.begin();

    iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
    output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
    Serial.println(output);
    checkIaqSensorStatus();

    bsec_virtual_sensor_t sensorList[10] = {
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };

    iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
    checkIaqSensorStatus();
  
    // Print the header
    output = "Timestamp [ms], raw temperature [°C], pressure [hPa], raw relative humidity [%], gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%], Static IAQ, CO2 equivalent, breath VOC equivalent";
    Serial.println(output);
}

void checkIaqSensorStatus(void) {
    if (iaqSensor.status != BSEC_OK) {
        if (iaqSensor.status < BSEC_OK) {
            output = "BSEC error code : " + String(iaqSensor.status);
            Serial.println(output);
            for (;;)
                errLeds(); /* Halt in case of failure */
        } else {
            output = "BSEC warning code : " + String(iaqSensor.status);
            Serial.println(output);
        }
    }

    if (iaqSensor.bme680Status != BME680_OK) {
        if (iaqSensor.bme680Status < BME680_OK) {
        output = "BME680 error code : " + String(iaqSensor.bme680Status);
        Serial.println(output);
        for (;;)
            errLeds(); /* Halt in case of failure */
        } else {
            output = "BME680 warning code : " + String(iaqSensor.bme680Status);
            Serial.println(output);
        }
    }
}

void errLeds() {
    // TODO Add led to indicate status of weather station.
}

void setupRouting(void) {
    server.on("/simple", getSimpleReadings);
    server.on("/all", getAllReadings);

    server.begin();
}

void createJson(char *tag, float value, char *unit) {
    jsonDocument.clear();
    jsonDocument["type"] = tag;
    jsonDocument["value"] = value;
    jsonDocument["unit"] = unit;
    serializeJson(jsonDocument, buffer);
}

void addJsonObject(char *tag, float value, char *unit) {
    JsonObject obj = jsonDocument.createNestedObject();
    obj["type"] = tag;
    obj["value"] = value;
    obj["unit"] = unit;
}

void readSensorData(void * parameter) {
    for (;;) {
        while (! iaqSensor.run()) { // If no data is available
            checkIaqSensorStatus();
            delay(100);
        }

        rawTempO = iaqSensor.rawTemperature;
        pressureO = iaqSensor.pressure / 100.0;
        rawHumidityO = iaqSensor.rawHumidity;
        gasO = iaqSensor.gasResistance;
        iaqO = iaqSensor.iaq;
        iaqAccuracyO = iaqSensor.iaqAccuracy;
        staticIaqO = iaqSensor.staticIaq;
        co2eO = iaqSensor.co2Equivalent;
        breathVoceO = iaqSensor.breathVocEquivalent;
        calTempO = iaqSensor.temperature;
        calHum0 = iaqSensor.humidity;
        
        // Sleep for POLLING_FREQUENCY seconds
        vTaskDelay(POLLING_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
}

void getSimpleReadings(void) {
    jsonDocument.clear();
    
    addJsonObject("temperature", calTempO, "°C");
    addJsonObject("humidity", calHum0, "%");
    addJsonObject("pressure", pressureO, "hPa");
    addJsonObject("iaq", iaqO, "score");
    
    serializeJson(jsonDocument, buffer);
    server.send(200, "application/json", buffer);
}


void getAllReadings(void) {
    jsonDocument.clear();
    
    output = "Timestamp [ms], raw temperature [°C], pressure [hPa], raw relative humidity [%], gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%], Static IAQ, CO2 equivalent, breath VOC equivalent";

    addJsonObject("raw_temperature", rawTempO, "°C");
    addJsonObject("raw_humidity", rawHumidityO, "%");
    addJsonObject("gas_resistance", gasO, "Ohm");
    addJsonObject("iaq_accuracy", iaqAccuracyO, "0-3");
    addJsonObject("static_iaq", staticIaqO, "score");
    addJsonObject("co2_equivalent", co2eO, "ppm");
    addJsonObject("breath_voc_equivalent", breathVoceO, "ppm");
    addJsonObject("temperature", calTempO, "°C");
    addJsonObject("humidity", calHum0, "%");
    addJsonObject("pressure", pressureO, "hPa");
    addJsonObject("iaq", iaqO, "score");

    serializeJson(jsonDocument, buffer);
    server.send(200, "application/json", buffer);
}

void setupSensorDataPoller(void) {
    // Create this task. When this task delays (sleeps), other parts of program
    // can run.
    xTaskCreate(
        readSensorData,
        "Read sensor data",
        10000,
        NULL,
        1,
        NULL
    );
}
