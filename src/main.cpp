#include "Log.h"
#include <Arduino.h>
#include <stdio.h>
#include <TFT_eSPI.h>
#include <Battery18650Stats.h>
#include "Button2.h"
#include "AppConfig.h"
#include "NTPTime.h"
#include "ESPNow.h"
#include <WiFi.h>
#define USE_LittleFS

#include <FS.h>
#ifdef USE_LittleFS
  #define SPIFFS LITTLEFS
  #include <LITTLEFS.h> 
  #define FORMAT_LITTLEFS_IF_FAILED true
#else
  #include <SPIFFS.h>
#endif 

#define ADC_EN                              14 //ADC_EN is the ADC detection enable port
#define ADC_PIN                             34
#define SENSOR_PIN                          12
#define CONV_FACTOR                          1.8
#define READS                               30
#define MIN_USB_VOL                          4.8 //volts
#define BUTTON_RIGHT                        35
#define BUTTON_LEFT                          0
#define TIME_STRING_LENGTH                 100 
#define MINIMUM_TIME_LONG_CLICK            200 //ms
#define DOMOTICZ_VOLTAGE_DEVICE_ID           6
#define DOMOTICZ_CHARGE_DEVICE_ID            7
#define BATTERY_INFO_UPDATE_INTERVAL        10 //seconds
#define DOMOTICZ_WATER_LEVEL_DEVICE_ID       8
#define WATER_LEVEL_INFO_UPDATE_INTERVAL    10 //seconds
#define DISPLAY_SLEEP_TIMEOUT                5 //seconds without interaction to turn off display
#define DEEP_SLEEP_TIMEOUT                   5 //seconds without interaction to start deep sleep
#define DEEP_SLEEP_WAKEUP                 1800 //seconds of deep sleeping for device to wake up

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

RTC_DATA_ATTR int bootCount = 0;

TaskHandle_t waterLevelTaskHandle;

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

TaskHandle_t updateTimeTaskHandle;
NTPTime ntpTime = NTPTime();

ESPNow espNow = ESPNow();

// ESPNow callback when data is sent
void ESPNow_OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  espNow.onDataSent(mac_addr, status);
}

void publishLogContent() {
  char* jsonStr = Log::readLogFileAsJsonPretty();
  espNow.sendMessage(std::string(jsonStr), LOG);
}

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
  vTaskDelay(ms / portTICK_PERIOD_MS);
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
  ESP_LOGI("MAIN", "Initiating deep sleep");
  ESP_LOGI("MAIN", "Will wakeup after %d seconds", DEEP_SLEEP_WAKEUP);
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
  ESP_LOGI("MAIN", "Boot number: %i", bootCount);
}
void printWakeupReason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : ESP_LOGI("MAIN", "Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : ESP_LOGI("MAIN", "Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : {
      ESP_LOGI("MAIN", "Wakeup caused by timer"); 
      turnOffDisplay();
      break;
    }
    case ESP_SLEEP_WAKEUP_TOUCHPAD : ESP_LOGI("MAIN", "Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : ESP_LOGI("MAIN", "Wakeup caused by ULP program"); break;
    default : ESP_LOGI("MAIN", "Wakeup was not caused by deep sleep: %s", String(wakeup_reason)); break;
  }
}

void display_sleep_task(void *args) {
  while(true) {
    ESP_LOGI("MAIN", "Display sleep timer: %d", displaySleepTimer);
    ESP_LOGI("MAIN", "Deep sleep timer: %d", deepSleepTimer);
    if (displaySleepTimer == 0) {
      turnOffDisplay();
      // resetDisplaySleepTimer();
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
  xTaskCreate(display_sleep_task, "display_sleep_task", 10000, NULL, tskIDLE_PRIORITY, &displaySleepTaskHandle);
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
    ntpTime.getTimeString(timeString, length);
    updateTimeInfo(timeString);
    taskDelay(1000);
  }
}
void createTimeTask() {
  xTaskCreate(update_time_task, "update_time_task", 2048, NULL, tskIDLE_PRIORITY, &updateTimeTaskHandle);
}
void resumeUpdateTimeTask() {
  vTaskResume(updateTimeTaskHandle);
}

void publishWaterLevelInfo(int waterLevel) {
  char waterLevelBuff[100];
  snprintf(waterLevelBuff, 100, "{\"idx\": %d, \"nvalue\": %d}", DOMOTICZ_WATER_LEVEL_DEVICE_ID, waterLevel);
  espNow.sendMessage(std::string(waterLevelBuff), SENSOR_INFO);
}
void printWaterLevelInfo() {
  if (waterLevelTaskHandle != NULL && eTaskGetState(waterLevelTaskHandle) == eSuspended) {
    ESP_LOGI("MAIN", "printWaterLevelInfo(): task is suspended");
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

    ESP_LOGI("MAIN", "Water Sensor Level: %d", waterLevel);

    updateWaterLevelInfo(waterLevel);
    publishWaterLevelInfo(waterLevel);
    taskDelay(WATER_LEVEL_INFO_UPDATE_INTERVAL * 1000);
  }
}
void createWaterLevelTask() {
  xTaskCreate(water_level_task, "water_level_task", 10000, NULL, tskIDLE_PRIORITY, &waterLevelTaskHandle);
}

void publishBatteryInfo(int batteryChargeLevel, double batteryVoltage) {
  char voltageBuff[100];
  char chargeBuff[100];
  snprintf(voltageBuff, 100, "{\"idx\": %d, \"nvalue\": 0, \"svalue\": \"%0.2f\"}", DOMOTICZ_VOLTAGE_DEVICE_ID, batteryVoltage);
  snprintf(chargeBuff, 100, "{\"idx\": %d, \"nvalue\": 0, \"svalue\": \"%d\"}", DOMOTICZ_CHARGE_DEVICE_ID, batteryChargeLevel);
  espNow.sendMessage(std::string(voltageBuff), SENSOR_INFO);
  espNow.sendMessage(std::string(chargeBuff), SENSOR_INFO);
}
void printBatteryInfo() {
  if (batteryInfoTaskHandle != NULL && eTaskGetState(batteryInfoTaskHandle) == eSuspended) {
    ESP_LOGI("MAIN", "printBatteryInfo(): task is suspended");
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

    ESP_LOGI("MAIN", "Volts: %.2f", batteryVoltage);
    ESP_LOGI("MAIN", "Charge level: %d", batteryChargeLevel);
    ESP_LOGI("MAIN", "Charge level (using the reference table): %d", battery.getBatteryChargeLevel(true));

    updateBatteryInfo(batteryChargeLevel, batteryVoltage);
    publishBatteryInfo(batteryChargeLevel, batteryVoltage);
    
    // Serial.printf("\nbattery_info_task() - Free Stack Space: %d\n", uxTaskGetStackHighWaterMark(NULL));
    taskDelay(BATTERY_INFO_UPDATE_INTERVAL * 1000);
  }
}
void suspendBatteryInfoTask() {
  if (batteryInfoTaskHandle == NULL) {
    ESP_LOGI("MAIN", "suspendBatteryInfoTask(): batteryInfoTask not yet created");
    return;
  }
  ESP_LOGI("MAIN", "Suspending batteryInfo task");
  vTaskSuspend(batteryInfoTaskHandle);
}
void createBatteryInfoTask() {
  if (batteryInfoTaskHandle != NULL) {
    ESP_LOGI("MAIN", "createBatteryInfoTask(): batteryInfoTask already created");
    return;
  }
  ESP_LOGI("MAIN", "Creating batteryInfo task ");
  xTaskCreate(battery_info_task, "battery_info_task", 10000, NULL, tskIDLE_PRIORITY, &batteryInfoTaskHandle);
}
void resumeBatteryInfoTask() {
  if (batteryInfoTaskHandle == NULL) {
    createBatteryInfoTask();
  } else {
    ESP_LOGI("MAIN", "Resuming batteryInfo task");
    vTaskResume(batteryInfoTaskHandle);
  }
}

void update_display_task(void *arg) {
  while(true) {
    // if (!isDisplayActive() || !tft.availableForWrite()) break;
    // printTime();
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
  xTaskCreate(update_display_task, "update_display_task", 10000, NULL, tskIDLE_PRIORITY, &updateDisplayTaskHandle);
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

boolean validateLongClick(Button2 &b) {
  unsigned int time = b.wasPressedFor();
  boolean validTime = time >= MINIMUM_TIME_LONG_CLICK;

  if (!validTime) {
    ESP_LOGI("MAIN", "Invalid longClick time: %dms", time);
    return false;
  }
  return true;
}
void button_init()
{
  leftButton.setLongClickHandler([](Button2 & b) {
    if (!isDisplayActive()) {
      wakeUpDisplay();
      return;
    }
    resetSleepTimers();
    if (!validateLongClick(b)) return;
    ESP_LOGI("MAIN", "Left button long click");
    ESP_LOGI("MAIN", "Go to Scan WIFI...");
    changeMenuOption(WIFI_SCAN);
    wifi_scan();
  });
  leftButton.setClickHandler([](Button2 & b) {
    if (!isDisplayActive()) {
      wakeUpDisplay();
      return;
    }
    resetSleepTimers();
    ESP_LOGI("MAIN", "Left button press");
    ESP_LOGI("MAIN", "Go to Water Level info..");
    changeMenuOption(WATER_LEVEL);
  });
  leftButton.setDoubleClickHandler([](Button2 & b) {
    ESP_LOGI("MAIN", "Truncating log file");
    Log::truncateLogFile();
  });

  rightButton.setLongClickHandler([](Button2 & b) {
    if (!isDisplayActive()) {
      wakeUpDisplay();
      return;
    }
    resetSleepTimers();
    if (!validateLongClick(b)) return;
    ESP_LOGI("MAIN", "Right button long click");
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
    ESP_LOGI("MAIN", "Right button click");
    ESP_LOGI("MAIN", "Go to Battery info..");
    changeMenuOption(BATTERY_INFO);
  });
  rightButton.setDoubleClickHandler([](Button2 & b) {
    ESP_LOGI("MAIN", "Publishing log file");
    publishLogContent();
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

int redirectToLittleFS(const char *szFormat, va_list args) {
  return Log::log(szFormat, args);
}

void logInit() {
  esp_log_set_vprintf(redirectToLittleFS);
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  Log::init();
}

void setup() {
  serialInit();
  logInit();
  displayInit();
  printBootCount();
  printWakeupReason();
  loadAppConfig();
  pinoutInit();
  changeMenuOption(INSTRUCTIONS);
  button_init();
  espNow.init(myConfig.espNowGatewayMacAddress, myConfig.wifiSSID);
  createTimeTask();
  createWaterLevelTask();
  createBatteryInfoTask();
  createUpdateDisplayTask();
  createDisplaySleepTask();
}

void loop() {
  button_loop();
}

