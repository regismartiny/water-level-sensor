#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Battery18650Stats.h>
#include "Button2.h"
#include "WiFi.h"
#include "EspMQTTClient.h"
#include <ESP32Time.h>
#include "AppConfig.h"

#define ADC_EN                              14 //ADC_EN is the ADC detection enable port
#define ADC_PIN                             34
#define SENSOR_PIN                          12
#define CONV_FACTOR                          1.8
#define READS                               30
#define MIN_USB_VOL                          4.8 //volts
#define BUTTON_RIGHT                        35
#define BUTTON_LEFT                          0
#define MINIMUM_TIME_LONG_CLICK            200 //ms
#define DOMOTICZ_TOPIC_SEND                "domoticz/in"
#define DOMOTICZ_TOPIC_LAST_WILL           "domoticz/out/lastwill"
#define DOMOTICZ_VOLTAGE_DEVICE_ID           6
#define DOMOTICZ_CHARGE_DEVICE_ID            7
#define BATTERY_INFO_UPDATE_INTERVAL        10 //seconds
#define TIME_STRING_LENGTH                 100 
#define DOMOTICZ_WATER_LEVEL_DEVICE_ID       8
#define WATER_LEVEL_INFO_UPDATE_INTERVAL    10 //seconds
#define DISPLAY_SLEEP_TIMEOUT               10 //seconds without interaction to turn off display
#define DEEP_SLEEP_TIMEOUT                  60 //seconds without interaction to start deep sleep
#define DEEP_SLEEP_WAKEUP                  600 //seconds of deep sleeping for device to wake up

RTC_DATA_ATTR int bootCount = 0;

TaskHandle_t waterLevelTaskHandle;

long gmtOffset_sec = -10800; // offset in seconds GMT-3
int daylightOffset_sec = 0;
const char* ntpServer = "br.pool.ntp.org";
ESP32Time rtc(gmtOffset_sec);
TaskHandle_t updateTimeTaskHandle;

enum MENUS { INSTRUCTIONS, WATER_LEVEL, WIFI_SCAN, BATTERY_INFO, DEEP_SLEEP };
struct {
  MENUS activeMenu = INSTRUCTIONS;
} myMenuInfo;

struct {
  char prevTime[TIME_STRING_LENGTH];
  char lastTime[TIME_STRING_LENGTH]; 
  boolean timeChanged;
  char timeOnDisplay[TIME_STRING_LENGTH];
  boolean enableDisplayInfo = true;
} myTimeInfo;

struct {
  int prevCharge;
  int lastCharge;
  boolean chargeChanged;
  int chargeOnDisplay = -1;
  double prevVoltage;
  double lastVoltage;
  boolean voltageChanged;
  double voltageOnDisplay = -1;
  boolean enableDisplayInfo = true;
} myBatteryInfo;

struct {
  int prevValue;
  int lastValue; 
  boolean valueChanged;
  int valueOnDisplay = -1;
  boolean enableDisplayInfo = true;
} myWaterLevelInfo;

TFT_eSPI tft = TFT_eSPI();
TaskHandle_t updateDisplayTaskHandle;
int displaySleepTimer = DISPLAY_SLEEP_TIMEOUT;
int deepSleepTimer = DEEP_SLEEP_TIMEOUT;
TaskHandle_t displaySleepTaskHandle;

Battery18650Stats battery(ADC_PIN, CONV_FACTOR, READS);
TaskHandle_t batteryInfoTaskHandle;

Button2 rightButton(BUTTON_RIGHT);
Button2 leftButton(BUTTON_LEFT);

char wifiNetworksBuff[512];

Config myConfig = Config();
AppConfig myAppConfig = AppConfig(&myConfig);

EspMQTTClient client = EspMQTTClient();

void serialInit() {
  Serial.begin(9600);
  delay(1000);
}
void displayInit() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setSwapBytes(true);
}
void clearDisplayDetailArea() {
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0, 20, 135, 240, TFT_BLACK);
}

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms)
{
  esp_sleep_enable_timer_wakeup(ms * 1000);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_light_sleep_start();
}
void taskDelay(int ms) {
  vTaskDelay(ms);
}
void showInstructions() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("LeftButton:", tft.width() / 2, tft.height() / 2 - 48);
  tft.drawString("[Water Level]", tft.width() / 2, tft.height() / 2 - 32 );
  tft.drawString("LeftButtonLongPress:", tft.width() / 2, tft.height() / 2 - 16);
  tft.drawString("[WiFi Scan]", tft.width() / 2, tft.height() / 2 );
  tft.drawString("RightButton:", tft.width() / 2, tft.height() / 2 + 16);
  tft.drawString("[Battery Info]", tft.width() / 2, tft.height() / 2 + 32 );
  tft.drawString("RightButtonLongPress:", tft.width() / 2, tft.height() / 2 + 48);
  tft.drawString("[Deep Sleep]", tft.width() / 2, tft.height() / 2 + 64 );
}
void changeMenuOption(MENUS menuOption) {
  clearDisplayDetailArea();
  switch (menuOption) {
    case INSTRUCTIONS:
      showInstructions();
      break;
    case WATER_LEVEL:
      myWaterLevelInfo.valueOnDisplay = -1;
      break;
    case BATTERY_INFO:
      myBatteryInfo.voltageOnDisplay = -1;
      myBatteryInfo.chargeOnDisplay = -1;
      break;
  }
  myMenuInfo.activeMenu = menuOption;
}

boolean isDisplayActive() {
  int r = digitalRead(TFT_BL);
  return r == 1;
}
void turnOffDisplay() {
  if (!isDisplayActive()) return;
  tft.fillScreen(TFT_BLACK);
  tft.flush();
  digitalWrite(TFT_BL, LOW);
  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);
}
void resetDisplaySleepTimer() {
  displaySleepTimer = DISPLAY_SLEEP_TIMEOUT;
}
void resetDeepSleepTimer() {
  deepSleepTimer = DEEP_SLEEP_TIMEOUT;
}
void resetSleepTimers() {
  resetDeepSleepTimer();
  resetDisplaySleepTimer();
}
void wakeUpDisplay() {
  if (!isDisplayActive()) {
    tft.writecommand(TFT_DISPON);
    displayInit();
    changeMenuOption(INSTRUCTIONS);
    digitalWrite(TFT_BL, HIGH);
    resetDisplaySleepTimer();
    delay(500);
  }
}
void goToDeepSleep() {
  Serial.println("Initiating deep sleep");
  Serial.printf("Will wakeup after %d seconds\n", DEEP_SLEEP_WAKEUP);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);
  delay(200);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_WAKEUP * 1000 * 1000);
  esp_deep_sleep_start();
}
void goToSleep() {
  // int r = digitalRead(TFT_BL);
  espDelay(6000);
  // digitalWrite(TFT_BL, !r);

  turnOffDisplay();
  //After using light sleep, you need to disable timer wake, because here use external IO port to wake up
  // esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  // esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
  goToDeepSleep();
}
void printBootCount() {
   //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
}
void printWakeupReason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void display_sleep_task(void *args) {
  while(true) {
    Serial.printf("Display sleep timer: %d\n", displaySleepTimer);
    Serial.printf("Deep sleep timer: %d\n", deepSleepTimer);
    if (displaySleepTimer == 0) {
      turnOffDisplay();
      resetDisplaySleepTimer();
    } else {
      displaySleepTimer--;
    }

    if (deepSleepTimer == 0) {
      resetDeepSleepTimer();
      goToDeepSleep();
    } else {
      deepSleepTimer--;
    }

    taskDelay(1000);
  }
}
void createDisplaySleepTask() {
  xTaskCreate(display_sleep_task, "display_sleep_task", 2048, NULL, tskIDLE_PRIORITY, &displaySleepTaskHandle);
}

void setTime() {
  /*---------set Time with NTP---------------*/
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)){
    Serial.println("Time received from NTP");
    Serial.printf("year: %d\n", 1900 + timeinfo.tm_year);
    Serial.printf("month: %d\n", 1 + timeinfo.tm_mon);
    Serial.printf("month day: %d\n", timeinfo.tm_mday);
    Serial.printf("week day: %c%c\n", "SMTWTFS"[timeinfo.tm_wday], "uouehra"[timeinfo.tm_wday]);
    Serial.printf("year day: %d\n", 1 + timeinfo.tm_yday);
    Serial.printf("hour: %d\n", timeinfo.tm_hour);
    Serial.printf("minute: %d\n", timeinfo.tm_min);
    Serial.printf("second: %d\n", timeinfo.tm_sec);
    rtc.setTimeStruct(timeinfo); 
  } else {
    Serial.println("Error getting time from NTP");
  }
}
void getTimeString(char* outStr, int length) {
  time_t t = time(NULL);
  struct tm *t_st;
  t_st = localtime(&t);
  snprintf(outStr, length, "%02d/%02d/%02d %02d:%02d:%02d", t_st->tm_mday, 1 + t_st->tm_mon, 
  abs(1900 + t_st->tm_year - 2000), t_st->tm_hour, t_st->tm_min, t_st->tm_sec);
}
void printTime() {
  if (myTimeInfo.timeChanged && strcmp(myTimeInfo.timeOnDisplay, myTimeInfo.lastTime) != 0) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(0, 0, 120, 30, TFT_BLACK);
    // tft.drawRect(0, 0, 120, 10, TFT_YELLOW);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(myTimeInfo.lastTime, 0, 0, 2);
  }
  strcpy(myTimeInfo.timeOnDisplay, myTimeInfo.lastTime);
}
void updateTimeInfo(const char* timeString) {
  if (strcmp(myTimeInfo.lastTime, timeString) != 0) {
    strcpy(myTimeInfo.prevTime, myTimeInfo.lastTime);
    myTimeInfo.timeChanged = true;
  } else {
    myTimeInfo.timeChanged = false;
  }
  strcpy(myTimeInfo.lastTime, timeString);
  // Serial.printf("\nmyTimeInfo.lastTime: %s", myTimeInfo.lastTime);
  // Serial.printf("\ntimeString: %s", timeString);
  // Serial.printf("\ntimeChanged: %s", myTimeInfo.timeChanged ? "true" : "false");
}
void update_time_task(void *arg) {
  int length = TIME_STRING_LENGTH * sizeof(char);
  char timeString[length];
  while(true) {
    getTimeString(timeString, length);
    updateTimeInfo(timeString);
    taskDelay(1000);
  }
}
void createTimeTask() {
  xTaskCreate(update_time_task, "update_time_task", 2048, NULL, tskIDLE_PRIORITY, &updateTimeTaskHandle);
  // vTaskSuspend(updateTimeTaskHandle);
}
void resumeUpdateTimeTask() {
  vTaskResume(updateTimeTaskHandle);
}

void publishWaterLevelInfo(int waterLevel) {
  if (!client.isConnected()) {
    Serial.println("publishWaterLevelInfo(): MQTT client is not connected.");
    return;
  }
  char waterLevelBuff[100];
  snprintf(waterLevelBuff, 100, "{\"idx\": %d, \"nvalue\": %d}", DOMOTICZ_WATER_LEVEL_DEVICE_ID, waterLevel);
  client.publish(DOMOTICZ_TOPIC_SEND, String(waterLevelBuff));
}
void printWaterLevelInfo() {
  if (eTaskGetState(waterLevelTaskHandle) == eSuspended) {
    Serial.println("printWaterLevelInfo(): task is suspended");
    return;
  }
  if (!myWaterLevelInfo.enableDisplayInfo) return;

  boolean updateValue = myWaterLevelInfo.valueOnDisplay == -1 || (myWaterLevelInfo.lastValue != myWaterLevelInfo.valueOnDisplay);
  if (updateValue) {
    int waterLevel = myWaterLevelInfo.lastValue;
    const char* waterLevelStr = waterLevel == 0 ? "OK" : "LOW";
    clearDisplayDetailArea();
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("Water Level is ", tft.width() / 2, tft.height() / 2, 2);
    tft.setTextColor(waterLevel == 0 ? TFT_GREEN : TFT_RED);
    tft.drawString(String(waterLevelStr), tft.width() / 2, tft.height() / 2 + 40, 4);
    myWaterLevelInfo.valueOnDisplay = waterLevel;
  }
}
void updateWaterLevelInfo(int waterLevel) {
   if (myWaterLevelInfo.lastValue != waterLevel) {
    myWaterLevelInfo.prevValue = myWaterLevelInfo.lastValue;
    myWaterLevelInfo.valueChanged = true;
  } else {
    myWaterLevelInfo.valueChanged = false;
  }
  myWaterLevelInfo.lastValue = waterLevel;
}
void water_level_task(void *arg) {
  while(true) {
    int waterLevel = digitalRead(SENSOR_PIN);

    Serial.print("\nWater Sensor Level: ");
    Serial.println(waterLevel);

    updateWaterLevelInfo(waterLevel);
    publishWaterLevelInfo(waterLevel);
    taskDelay(WATER_LEVEL_INFO_UPDATE_INTERVAL * 1000);
  }
}
void createWaterLevelTask() {
  xTaskCreate(water_level_task, "water_level_task", 2048, NULL, tskIDLE_PRIORITY, &waterLevelTaskHandle);
  // waterLevelTaskHandle(updateTimeTaskHandle);
}

void publishBatteryInfo(int batteryChargeLevel, double batteryVoltage) {
  if (!client.isConnected()) {
    Serial.println("publishBatteryInfo(): MQTT client is not connected.");
    return;
  }
  char voltageBuff[100];
  char chargeBuff[100];
  snprintf(voltageBuff, 100, "{\"idx\": %d, \"nvalue\": 0, \"svalue\": \"%0.2f\"}", DOMOTICZ_VOLTAGE_DEVICE_ID, batteryVoltage);
  snprintf(chargeBuff, 100, "{\"idx\": %d, \"nvalue\": 0, \"svalue\": \"%d\"}", DOMOTICZ_CHARGE_DEVICE_ID, batteryChargeLevel);
  client.publish(DOMOTICZ_TOPIC_SEND, String(voltageBuff));
  client.publish(DOMOTICZ_TOPIC_SEND, String(chargeBuff));
}
void printBatteryInfo() {
  if (eTaskGetState(batteryInfoTaskHandle) == eSuspended) {
    Serial.println("printBatteryInfo(): task is suspended");
    return;
  }
  if (!myBatteryInfo.enableDisplayInfo) {
    return;
  }
  boolean updateVoltage = myBatteryInfo.voltageOnDisplay == -1 || (myBatteryInfo.lastVoltage != myBatteryInfo.voltageOnDisplay);
  boolean updateCharge = updateVoltage || myBatteryInfo.chargeOnDisplay == -1 || (myBatteryInfo.lastCharge != myBatteryInfo.chargeOnDisplay);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_RED);
  tft.drawString("Nivel de carga", 10, 30, 2);
  if (updateCharge) {
    tft.setTextColor(TFT_GREEN);
    // tft.drawRect(10, 60, 100, 30, TFT_YELLOW);
    tft.fillRect(10, 60, 100, 30, TFT_BLACK);
    boolean charging = myBatteryInfo.lastVoltage >= MIN_USB_VOL;
    if (charging) {
      tft.drawString("carregando...", 10, 60, 2);
    } else {
      tft.drawString(String(myBatteryInfo.lastCharge) + "%", 10, 60, 4);
    }
    myBatteryInfo.chargeOnDisplay = myBatteryInfo.lastCharge;
  }

  tft.setTextColor(TFT_RED);
  tft.drawString("Voltagem", 10, 90, 2);
  if (updateVoltage) {
    tft.setTextColor(TFT_BLUE);
    // tft.drawRect(10, 115, 100, 30, TFT_YELLOW);
    tft.fillRect(10, 115, 100, 30, TFT_BLACK);
    tft.drawString(String(myBatteryInfo.lastVoltage) + "V", 10, 115, 4);
    myBatteryInfo.voltageOnDisplay = myBatteryInfo.lastVoltage;
  }
}
void updateBatteryInfo(int batteryChargeLevel, double batteryVoltage) {
  if (myBatteryInfo.lastCharge != batteryChargeLevel) {
    myBatteryInfo.prevCharge = myBatteryInfo.lastCharge;
    myBatteryInfo.chargeChanged = true;
  } else {
     myBatteryInfo.chargeChanged = false;
  }
  if (myBatteryInfo.lastVoltage != batteryVoltage) {
    myBatteryInfo.prevVoltage = myBatteryInfo.lastVoltage;
    myBatteryInfo.voltageChanged = true;
  } else {
    myBatteryInfo.voltageChanged = false;
  }
  myBatteryInfo.lastCharge = batteryChargeLevel;
  myBatteryInfo.lastVoltage = batteryVoltage;
  // Serial.printf("\nmyBatteryInfo.lastCharge %d", myBatteryInfo.lastCharge);
  // Serial.printf("\nmyBatteryInfo.prevCharge %d", myBatteryInfo.prevCharge);
  // Serial.printf("\nmyBatteryInfo.lastVoltage %f", myBatteryInfo.lastVoltage);
  // Serial.printf("\nmyBatteryInfo.prevVoltage %f", myBatteryInfo.prevVoltage);
  // Serial.printf("\nmyBatteryInfo.chargeChanged %s", myBatteryInfo.chargeChanged ? "true" : "false");
  // Serial.printf("\nmyBatteryInfo.voltageChanged %s", myBatteryInfo.voltageChanged ? "true" : "false");
}
void battery_info_task(void *arg) {
  while(true) {
    int batteryChargeLevel = battery.getBatteryChargeLevel();
    double batteryVoltage = battery.getBatteryVolts();

    Serial.print("Volts: ");
    Serial.println(batteryVoltage);

    Serial.print("Charge level: ");
    Serial.println(batteryChargeLevel);

    Serial.print("Charge level (using the reference table): ");
    Serial.println(battery.getBatteryChargeLevel(true));

    updateBatteryInfo(batteryChargeLevel, batteryVoltage);
    publishBatteryInfo(batteryChargeLevel, batteryVoltage);
    
    // Serial.printf("\nbattery_info_task() - Free Stack Space: %d\n", uxTaskGetStackHighWaterMark(NULL));
    taskDelay(BATTERY_INFO_UPDATE_INTERVAL * 1000);
  }
}
void suspendBatteryInfoTask() {
  if (batteryInfoTaskHandle == NULL) {
    Serial.println("BatteryInfoTask not yet created");
    return;
  }
  Serial.println("Suspending batteryInfo task");
  vTaskSuspend(batteryInfoTaskHandle);
}
void createBatteryInfoTask() {
  if (batteryInfoTaskHandle != NULL) {
    Serial.println("BatteryInfoTask already created");
    return;
  }
  Serial.println("Creating batteryInfo task ");
  xTaskCreate(battery_info_task, "battery_info_task", 2048, NULL, tskIDLE_PRIORITY, &batteryInfoTaskHandle);
}
void resumeBatteryInfoTask() {
  if (batteryInfoTaskHandle == NULL) {
    createBatteryInfoTask();
  } else {
    Serial.println("Resuming batteryInfo task");
    vTaskResume(batteryInfoTaskHandle);
  }
}

void update_display_task(void *arg) {
  while(true) {
    // Serial.println("update_display_task loop");
    printTime();
    switch (myMenuInfo.activeMenu) {
      case BATTERY_INFO:
        printBatteryInfo();
        break;
      case WATER_LEVEL:
        printWaterLevelInfo();
        break;
      case INSTRUCTIONS:
      case WIFI_SCAN:
      case DEEP_SLEEP:
      default:
        break;
        // Serial.println("Active menu has no value to display");
    }
    taskDelay(500);
  }
}
void createUpdateDisplayTask() {
  xTaskCreate(update_display_task, "update_display_task", 2048, NULL, tskIDLE_PRIORITY, &updateDisplayTaskHandle);
}

void pinoutInit() {
  pinMode(ADC_EN, OUTPUT);
  digitalWrite(ADC_EN, HIGH);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
}

void wifi_scan()
{
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);

    tft.drawString("Scan Network", tft.width() / 2, tft.height() / 2, 2);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int16_t n = WiFi.scanNetworks();
    tft.setTextDatum(MC_DATUM);
    tft.fillScreen(TFT_BLACK);
    if (n == 0) {
        tft.drawString("no networks found", tft.width() / 2, tft.height() / 2);
    } else {
        tft.setCursor(0, 30);
        Serial.printf("Found %d net\n", n);
        for (int i = 0; i < n; ++i) {
            sprintf(wifiNetworksBuff,
                    "[%d]:%s(%d)",
                    i + 1,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i));
            tft.println(wifiNetworksBuff);
        }
    }
    // WiFi.mode(WIFI_OFF);
}

void connectWifi() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);

  tft.drawString("Connecting to " + String(myConfig.wifiSSID), tft.width() / 2, tft.height() / 2);

  WiFi.mode(WIFI_STA);
  WiFi.begin(myConfig.wifiSSID, myConfig.wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  tft.setTextDatum(MC_DATUM);
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Connected to " + String(myConfig.wifiSSID), tft.width() / 2, tft.height() / 2);
  tft.drawString("IP: " + WiFi.localIP().toString(), tft.width() / 2, tft.height() / 2 + 50);
  Serial.println(WiFi.localIP());
}

void mqttInit()
{
  client.setMqttClientName(myConfig.mqttClientName);
  client.setMqttServer(myConfig.mqttServer, myConfig.mqttUser, myConfig.mqttPassword, myConfig.mqttPort);
  client.setWifiCredentials(myConfig.wifiSSID, myConfig.wifiPassword);
  // Optional functionalities of EspMQTTClient
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.enableLastWillMessage(DOMOTICZ_TOPIC_LAST_WILL, "going offline");  // You can activate the retain flag by setting the third parameter to true
}
// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  setTime();
  // Subscribe to "domoticz/out" and display received message to Serial
  // client.subscribe("domoticz/out", [](const String& payload) {
  //   Serial.println("New message from domoticz/out: " + payload);
  // });

  // Subscribe to "mytopic/wildcardtest/#" and display received message to Serial
  // client.subscribe("domoticz/out/#", [](const String & topic, const String & payload) {
  //   Serial.println("(From wildcard) topic: " + topic + ", payload: " + payload);
  // });

  // Publish a message to "mytopic/test"
  client.publish("domoticz/out/test", "This is a message from WaterLevelSensor"); // You can activate the retain flag by setting the third parameter to true

  // Execute delayed instructions
  client.executeDelayed(5 * 1000, []() {
    client.publish("domoticz/out/test123", "This is a message from WaterLevelSensor sent 5 seconds later");
  });
}

boolean validateLongClick(Button2 &b) {
  unsigned int time = b.wasPressedFor();
  boolean validTime = time >= MINIMUM_TIME_LONG_CLICK;

  if (!validTime) {
    Serial.println("Invalid longClick time: " + String(time) + "ms");
    return false;
  }
  return true;
}
void button_init()
{
  rightButton.setLongClickHandler([](Button2 & b) {
    if (!isDisplayActive()) {
      wakeUpDisplay();
      return;
    }
    resetSleepTimers();
    if (!validateLongClick(b)) return;
    Serial.println("Right button long click");
    changeMenuOption(DEEP_SLEEP);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Press again to wake up",  tft.width() / 2, tft.height() / 2 );
    goToSleep();
  });
  rightButton.setClickHandler([](Button2 & b) {
    if (!isDisplayActive()) {
      wakeUpDisplay();
      return;
    }
    resetSleepTimers();
    Serial.println("Right button press");
    Serial.println("Show Battery Info..");
    changeMenuOption(BATTERY_INFO);
  });

  leftButton.setLongClickHandler([](Button2 & b) {
    if (!isDisplayActive()) {
      wakeUpDisplay();
      return;
    }
    resetSleepTimers();
    if (!validateLongClick(b)) return;
    Serial.println("Left button long click");
    Serial.println("Scan WIFI");
    changeMenuOption(WIFI_SCAN);
    wifi_scan();
  });
  leftButton.setClickHandler([](Button2 & b) {
    if (!isDisplayActive()) {
      wakeUpDisplay();
      return;
    }
    resetSleepTimers();
    Serial.println("Left button press");
    Serial.println("Show Water Level..");
    changeMenuOption(WATER_LEVEL);
  });
}
void button_loop()
{
  rightButton.loop();
  leftButton.loop();
}

void loadAppConfig() {
  myAppConfig.loadConfig();
}

void setup() {
  serialInit();
  printBootCount();
  printWakeupReason();
  loadAppConfig();
  pinoutInit();
  displayInit();
  changeMenuOption(INSTRUCTIONS);
  button_init();
  mqttInit();
  createTimeTask();
  createWaterLevelTask();
  createBatteryInfoTask();
  createUpdateDisplayTask();
  createDisplaySleepTask();
}

void loop() {
  button_loop();
  client.loop();
}

