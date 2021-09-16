void setupWiFi(void);
void setupBME680(void);
void checkIaqSensorStatus(void);
void errLeds(void);

void setupRouting(void);
void createJson(char *tag, float value, char *unit);
void addJsonObject(char *tag, float value, char *unit);
void readSensorData(void * parameter);

void getSimpleReadings(void);
void getAllReadings(void);
void setupSensorDataPoller(void);
