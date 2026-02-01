/*
 WiFi Web Air Quality Monitor.

 A Web server that displays readings from the AQN sensors.

 These currently include the:
 BME 680 (indoor temperature, humidity, pressure, air quality).
 DS18B20 (outdoor temperature),

 Circuit:
 BME 680 SDA -> D21, SCL -> D22
 DS18B20 DAT -> RX2
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <bsec2.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_task_wdt.h>
#include "main.h"

// You must create a settings.h file as per the README.
#include "settings.h"

// Device will reset if unresponsive for this long.
#define WDT_TIMEOUT_S 30

// WiFi connection settings.
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RECONNECT_INTERVAL_MS 60000

// Number of seconds between the polling of each sensor.
// DS18B20 might be acting weirdly if it is not polled quickly - but might also
// if the sensors are polled at approximately the same time???
#define DS18B20_POLLING_FREQUENCY 30
// Keep this low as it seems BME680 wants to be polled frequently (even if data)
// is not available yet - the BSEC 2 example has a continuous loop with no
// delay.
#define BME680_POLLING_FREQUENCY 0.1

#define ONE_WIRE_BUS 16
#define DS18B20_TEMPERATURE_PRECISION 10

#define DS18B20_TEMPERATURE_OFFSET 0.1
#define BME680_TEMPERATURE_OFFSET 0.8
#define BME680_HUMIDITY_OFFSET 0.0
#define BME680_PRESSURE_OFFSET 0.0

#define PANIC_LED 2
#define ERROR_DUR 1000
#define BME_680_SAMPLE_RATE BSEC_SAMPLE_RATE_LP

WebServer server(PORT);

Bsec2 iaqSensor;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

StaticJsonDocument<1024> jsonDocument;
char buffer[1024];

// Sensor readings stored as char arrays to avoid heap fragmentation.
// DS18B20
char tempProbeOneO[16] = "";

// BME680
char rawTempO[16] = "";
char pressureO[16] = "";
char rawHumidityO[16] = "";
char gasO[16] = "";
char iaqO[16] = "";
char staticIaqO[16] = "";
char co2eO[16] = "";
char breathVoceO[16] = "";
char calTempO[16] = "";
char calHum0[16] = "";
char stabilizationStatus0[16] = "";
char runInStatus0[16] = "";
char iaqAccuracyO[16] = "";

SemaphoreHandle_t sensorDataMutex;

void setup()
{
    Serial.begin(115200);
    sensorDataMutex = xSemaphoreCreateMutex();

    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    setupWiFi();
    setupBME680();
    setupDS18B20();

    setupSensorDataPoller();
    setupRouting();
}

void loop()
{
    esp_task_wdt_reset();
    server.handleClient();
    yield();
}

bool setupWiFi()
{
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startAttemptTime >= WIFI_CONNECT_TIMEOUT_MS)
        {
            Serial.println("");
            Serial.println("WiFi connection timeout.");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
}

void setupDS18B20()
{
    DeviceAddress tempProbeOneAddress;

    sensors.begin();

    Serial.print("Locating devices...");

    Serial.print("Found ");
    Serial.print(sensors.getDeviceCount(), DEC);
    Serial.println(" OneWire devices.");

    Serial.print("Parasite power is: ");
    if (sensors.isParasitePowerMode())
        Serial.println("ON");
    else
        Serial.println("OFF");

    if (sensors.getAddress(tempProbeOneAddress, 0))
    {
        Serial.print("Found device ");
        Serial.print(0, DEC);
        Serial.println();

        Serial.print("Setting resolution to ");
        Serial.println(DS18B20_TEMPERATURE_PRECISION, DEC);

        sensors.setResolution(tempProbeOneAddress, DS18B20_TEMPERATURE_PRECISION);

        Serial.print("Resolution actually set to: ");
        Serial.print(sensors.getResolution(tempProbeOneAddress), DEC);
        Serial.println();
    }
    else
    {
        Serial.print("Found ghost device at ");
        Serial.print(0, DEC);
        Serial.print(" but could not detect address. Check power and cabling");
    }
}

void setupBME680()
{
    bsecSensor sensorList[] = {
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_STABILIZATION_STATUS,
        BSEC_OUTPUT_RUN_IN_STATUS,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };

    Wire.begin();
    pinMode(PANIC_LED, OUTPUT);

    // Valid for boards with USB-COM. Wait until the port is open.
    while (!Serial)
        delay(10);

    // Initialize the library and interfaces.
    if (!iaqSensor.begin(BME68X_I2C_ADDR_HIGH, Wire))
    {
        checkIaqSensorStatus(iaqSensor);
    }

    if (BME_680_SAMPLE_RATE == BSEC_SAMPLE_RATE_ULP)
    {
        iaqSensor.setTemperatureOffset(TEMP_OFFSET_ULP);
    }
    else if (BME_680_SAMPLE_RATE == BSEC_SAMPLE_RATE_LP)
    {
        iaqSensor.setTemperatureOffset(TEMP_OFFSET_LP);
    }

    if (!iaqSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), BME_680_SAMPLE_RATE))
    {
        checkIaqSensorStatus(iaqSensor);
    }

    iaqSensor.attachCallback(bme680Callback);

    Serial.println("BSEC library version " +
                   String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix));
}

void checkIaqSensorStatus(Bsec2 bsec)
{
    if (bsec.status < BSEC_OK)
    {
        Serial.println("BSEC error code : " + String(bsec.status));
        // Halt in case of failure.
        errLeds();
    }
    else if (bsec.status > BSEC_OK)
    {
        Serial.println("BSEC warning code : " + String(bsec.status));
    }

    if (bsec.sensor.status < BME68X_OK)
    {
        Serial.println("BME68X error code : " + String(bsec.sensor.status));
        // Halt in case of failure.
        errLeds();
    }
    else if (bsec.sensor.status > BME68X_OK)
    {
        Serial.println("BME68X warning code : " + String(bsec.sensor.status));
    }
}

void errLeds()
{
    while (1)
    {
        digitalWrite(PANIC_LED, HIGH);
        delay(ERROR_DUR);
        digitalWrite(PANIC_LED, LOW);
        delay(ERROR_DUR);
    }
}

void bme680Callback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
    if (!outputs.nOutputs)
    {
        return;
    }

    if (xSemaphoreTake(sensorDataMutex, (TickType_t)10) == pdTRUE)
    {
        for (uint8_t i = 0; i < outputs.nOutputs; i++)
        {
            const bsecData output = outputs.output[i];
            switch (output.sensor_id)
            {
            case BSEC_OUTPUT_IAQ:
                dtostrf(output.signal, 1, 2, iaqO);
                snprintf(iaqAccuracyO, sizeof(iaqAccuracyO), "%d", (int)output.accuracy);
                break;
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                dtostrf(output.signal, 1, 2, rawTempO);
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                dtostrf(output.signal + BME680_PRESSURE_OFFSET, 1, 2, pressureO);
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:
                dtostrf(output.signal, 1, 2, rawHumidityO);
                break;
            case BSEC_OUTPUT_RAW_GAS:
                dtostrf(output.signal, 1, 2, gasO);
                break;
            case BSEC_OUTPUT_STABILIZATION_STATUS:
                dtostrf(output.signal, 1, 0, stabilizationStatus0);
                break;
            case BSEC_OUTPUT_RUN_IN_STATUS:
                dtostrf(output.signal, 1, 0, runInStatus0);
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
                dtostrf(output.signal + BME680_TEMPERATURE_OFFSET, 1, 2, calTempO);
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
                dtostrf(output.signal + BME680_HUMIDITY_OFFSET, 1, 2, calHum0);
                break;
            case BSEC_OUTPUT_STATIC_IAQ:
                dtostrf(output.signal, 1, 2, staticIaqO);
                break;
            case BSEC_OUTPUT_CO2_EQUIVALENT:
                dtostrf(output.signal, 1, 2, co2eO);
                break;
            case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
                dtostrf(output.signal, 1, 2, breathVoceO);
                break;
            default:
                break;
            }
        }
        xSemaphoreGive(sensorDataMutex);
    }
}

void setupRouting()
{
    server.enableCORS();
    server.on("/simple", getSimpleReadings);
    server.on("/all", getAllReadings);

    server.begin();
}

void addJsonObject(const char *tag, const char *value, const char *unit)
{
    JsonObject obj = jsonDocument.createNestedObject(tag);
    obj["value"] = value;
    obj["unit"] = unit;
}

void readDS18B20SensorData(void *parameter)
{
    for (;;)
    {
        sensors.requestTemperatures();
        float temp = sensors.getTempCByIndex(0);

        if (temp == DEVICE_DISCONNECTED_C)
        {
            Serial.println("Error: Could not read temperature data");
        }
        else if (xSemaphoreTake(sensorDataMutex, (TickType_t)10) == pdTRUE)
        {
            dtostrf(temp + DS18B20_TEMPERATURE_OFFSET, 1, 2, tempProbeOneO);
            xSemaphoreGive(sensorDataMutex);
        }

        vTaskDelay(DS18B20_POLLING_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
}

void readBME680SensorData(void *parameter)
{
    for (;;)
    {
        if (!iaqSensor.run())
        {
            checkIaqSensorStatus(iaqSensor);
        }
        vTaskDelay(BME680_POLLING_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
}

void checkWiFiConnection(void *parameter)
{
    for (;;)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("WiFi disconnected, attempting to reconnect...");
            WiFi.disconnect();
            if (!setupWiFi())
            {
                Serial.println("Reconnection failed, will retry later.");
            }
        }
        vTaskDelay(WIFI_RECONNECT_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

void getSimpleReadings()
{
    jsonDocument.clear();

    if (xSemaphoreTake(sensorDataMutex, (TickType_t)10) == pdTRUE)
    {
        addJsonObject("indoor_temperature", calTempO, "°C");
        addJsonObject("outdoor_temperature", tempProbeOneO, "°C");
        addJsonObject("humidity", calHum0, "%");
        addJsonObject("pressure", pressureO, "hPa");
        addJsonObject("iaq", iaqO, "score");
        xSemaphoreGive(sensorDataMutex);
    }

    serializeJson(jsonDocument, buffer);
    server.send(200, "application/json", buffer);
}

void getAllReadings()
{
    jsonDocument.clear();
    if (xSemaphoreTake(sensorDataMutex, (TickType_t)10) == pdTRUE)
    {

        addJsonObject("raw_temperature", rawTempO, "°C");
        addJsonObject("raw_humidity", rawHumidityO, "%");
        addJsonObject("gas_resistance", gasO, "Ohm");
        addJsonObject("iaq_accuracy", iaqAccuracyO, "0-3");
        addJsonObject("static_iaq", staticIaqO, "score");
        addJsonObject("co2_equivalent", co2eO, "ppm");
        addJsonObject("breath_voc_equivalent", breathVoceO, "ppm");
        addJsonObject("indoor_temperature", calTempO, "°C");
        addJsonObject("outdoor_temperature", tempProbeOneO, "°C");
        addJsonObject("stabilization_status", stabilizationStatus0, "");
        addJsonObject("run_in_status", runInStatus0, "");
        addJsonObject("humidity", calHum0, "%");
        addJsonObject("pressure", pressureO, "hPa");
        addJsonObject("iaq", iaqO, "score");
        xSemaphoreGive(sensorDataMutex);
    }

    serializeJson(jsonDocument, buffer);
    server.send(200, "application/json", buffer);
}

void setupSensorDataPoller()
{
    // Create task. When this task delays (sleeps), other parts of program can
    // run.
    xTaskCreate(
        readBME680SensorData,
        "Read BME680 sensor data",
        8192,
        NULL,
        1,
        NULL);

    // A little wait between tasks.
    delay(1500);

    // The DB18B20 task seems to need a higher priority to avoid weird bugs when
    // reading the sensor data.
    xTaskCreate(
        readDS18B20SensorData,
        "Read DS18B20 sensor data",
        4096,
        NULL,
        2,
        NULL);

    delay(1500);

    xTaskCreate(
        checkWiFiConnection,
        "Reconnect to WiFi if connection drops",
        4096,
        NULL,
        1,
        NULL);
}
