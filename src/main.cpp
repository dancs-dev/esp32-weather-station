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
#include <OneWire.h>
#include <DallasTemperature.h>

#include "main.h"

// You must create a settings.h file as per the README.
#include "settings.h"

// Number of seconds between the polling of each sensor.
// DS18B20 might be acting weirdly if it is not polled quickly - but might also
// if the sensors are polled at approximately the same time???
#define DS18B20_POLLING_FREQUENCY 5
#define BME680_POLLING_FREQUENCY 15

#define ONE_WIRE_BUS 16
#define TEMPERATURE_PRECISION 9

WebServer server(80);

Bsec iaqSensor;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

StaticJsonDocument<2500> jsonDocument;
char buffer[2500];

String output;

// Sensor readings to be stored as global variables.
// DS18B20
float tempProbeOneO;

// BME680
float rawTempO, pressureO, rawHumidityO, gasO, iaqO, staticIaqO, co2eO,
    breathVoceO, calTempO, calHum0;
int iaqAccuracyO;

void setup() {
    Serial.begin(115200);
    
    setupWiFi();
    setupBME680();
    setupDS18B20();

    setupSensorDataPoller();
    setupRouting();
}

void loop(){
    server.handleClient();
}

void setupWiFi() {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setupDS18B20() {
    DeviceAddress tempProbeOneAddress;

    sensors.begin();

    Serial.print("Locating devices...");

    Serial.print("Found ");
    Serial.print(sensors.getDeviceCount(), DEC);
    Serial.println(" OneWire devices.");

    Serial.print("Parasite power is: "); 
    if (sensors.isParasitePowerMode()) Serial.println("ON");
    else Serial.println("OFF");

    if(sensors.getAddress(tempProbeOneAddress, 0)) {
        Serial.print("Found device ");
        Serial.print(0, DEC);
        Serial.println();

        Serial.print("Setting resolution to ");
        Serial.println(TEMPERATURE_PRECISION, DEC);

        sensors.setResolution(tempProbeOneAddress, TEMPERATURE_PRECISION);
        
        Serial.print("Resolution actually set to: ");
        Serial.print(sensors.getResolution(tempProbeOneAddress), DEC); 
        Serial.println();
    }
    else {
        Serial.print("Found ghost device at ");
        Serial.print(0, DEC);
        Serial.print(" but could not detect address. Check power and cabling");
    }

}

void setupBME680() {
    SPI.begin();
    iaqSensor.begin(5, SPI);
    output = "\nBSEC library version " + String(iaqSensor.version.major) + "." +
        String(iaqSensor.version.minor) + "." +
        String(iaqSensor.version.major_bugfix) + "." +
        String(iaqSensor.version.minor_bugfix);

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
  
    // The data that is accessible from the device + units
    // raw temperature [°C], pressure [hPa], raw relative humidity [%],
    // gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%],
    // Static IAQ, CO2 equivalent, breath VOC equivalent"
}

void checkIaqSensorStatus() {
    if (iaqSensor.status != BSEC_OK) {
        if (iaqSensor.status < BSEC_OK) {
            output = "BSEC error code : " + String(iaqSensor.status);
            Serial.println(output);
            for (;;)
                // Halt and display error leds in case of failure
                errLeds(); 
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
            errLeds();
        } else {
            output = "BME680 warning code : " + String(iaqSensor.bme680Status);
            Serial.println(output);
        }
    }
}

void errLeds() {
    // TODO Add led to indicate status of weather station.
}

void setupRouting() {
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

void readDS18B20SensorData(void * parameter) {
    for (;;) {
        // Serial.print("Requesting temperatures...");
        sensors.requestTemperatures();
        // Serial.println("DONE");

        tempProbeOneO = sensors.getTempCByIndex(0);
        if(tempProbeOneO == DEVICE_DISCONNECTED_C) 
        {
            Serial.println("Error: Could not read temperature data");
        }

        // Serial.print("Temperature for device: ");
        // Serial.println(tempProbeOneO);

        vTaskDelay(DS18B20_POLLING_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
}

void readBME680SensorData(void * parameter) {
    for (;;) {
        // If no data is available, loop until it is.
        while (! iaqSensor.run()) {
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

        vTaskDelay(BME680_POLLING_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
}

void getSimpleReadings() {
    jsonDocument.clear();
    
    addJsonObject("indoor_temperature", calTempO, "°C");
    addJsonObject("outdoor_temperature", tempProbeOneO, "°C");
    addJsonObject("humidity", calHum0, "%");
    addJsonObject("pressure", pressureO, "hPa");
    addJsonObject("iaq", iaqO, "score");
    
    serializeJson(jsonDocument, buffer);
    server.send(200, "application/json", buffer);
}


void getAllReadings() {
    jsonDocument.clear();

    addJsonObject("raw_temperature", rawTempO, "°C");
    addJsonObject("raw_humidity", rawHumidityO, "%");
    addJsonObject("gas_resistance", gasO, "Ohm");
    addJsonObject("iaq_accuracy", iaqAccuracyO, "0-3");
    addJsonObject("static_iaq", staticIaqO, "score");
    addJsonObject("co2_equivalent", co2eO, "ppm");
    addJsonObject("breath_voc_equivalent", breathVoceO, "ppm");
    addJsonObject("indoor_temperature", calTempO, "°C");
    addJsonObject("outdoor_temperature", tempProbeOneO, "°C");
    addJsonObject("humidity", calHum0, "%");
    addJsonObject("pressure", pressureO, "hPa");
    addJsonObject("iaq", iaqO, "score");

    serializeJson(jsonDocument, buffer);
    server.send(200, "application/json", buffer);
}

void setupSensorDataPoller() {
    // Create task. When this task delays (sleeps), other parts of program can
    // run.
    xTaskCreate(
        readBME680SensorData,
        "Read BME680 sensor data",
        10000,
        NULL,
        1,
        NULL
    );

    // The DB18B20 task seems to need a higher priority to avoid weird bugs when
    // reading the sensor data.
    xTaskCreate(
        readDS18B20SensorData,
        "Read DS18B20 sensor data",
        10000,
        NULL,
        2,
        NULL
    );
}
