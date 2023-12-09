#include <Arduino.h>
#include <stdio.h>
#include <Battery18650Stats.h>
#include "Button2.h"
#include "AppConfig.h"
#include "NTPTime.h"
#include "ESPNow.h"
#include <WiFi.h>
#include "PersistentLog.h"
#include "ESPLogMacros.h"
#include "Display.h"

#define LOW_POWER_MODE
// #define DISPLAY_ENABLED
// #define NTP_TIME_ENABLED
#define LOG_LEVEL                           ESP_LOG_VERBOSE
#define ADC_EN                              14 //ADC_EN is the ADC detection enable port
#define ADC_PIN                             34
#define SENSOR_PIN                          12
#define CONV_FACTOR                          1.8
#define READS                               30
#define MIN_USB_VOL                          4.8 //volts
#define TIME_STRING_LENGTH                 100 
#define MINIMUM_TIME_LONG_CLICK            200 //ms
#define DOMOTICZ_VOLTAGE_DEVICE_ID           6
#define DOMOTICZ_CHARGE_DEVICE_ID            7
#define BATTERY_INFO_UPDATE_INTERVAL        10 //seconds
#define DOMOTICZ_WATER_LEVEL_DEVICE_ID       8
#define WATER_LEVEL_INFO_UPDATE_INTERVAL    10 //seconds
#define DEEP_SLEEP_TIMEOUT                  15 //seconds without interaction to start deep sleep
#define LOG_TAG_MAIN                        "MAIN"

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

int deepSleepTimer = DEEP_SLEEP_TIMEOUT;

RTC_DATA_ATTR int bootCount = 0;

TaskHandle_t waterLevelTaskHandle;

TaskHandle_t deepSleepTaskHandle;


Battery18650Stats battery(ADC_PIN, CONV_FACTOR, READS);
TaskHandle_t batteryInfoTaskHandle;

Button2 rightButton(BUTTON_RIGHT);
Button2 leftButton(BUTTON_LEFT);

char wifiNetworksBuff[512];

PersistentLog persistentLog = PersistentLog();

Config myConfig = Config();
AppConfig myAppConfig = AppConfig(&myConfig);

ESPNow espNow = ESPNow();

#ifdef NTP_TIME_ENABLED
TaskHandle_t updateTimeTaskHandle;
NTPTime ntpTime = NTPTime();
#endif

#ifdef DISPLAY_ENABLED
void update_display_task(void *args);
void onDisplayWakeUp();
Display display = Display(&display_sleep_task, &update_display_task, &onDisplayWakeUp);
#endif

void PRINT(String str) {
  #ifndef LOW_POWER_MODE
  Serial.print(str);
  #endif
}

void PRINTLN(String str) {
  #ifndef LOW_POWER_MODE
  Serial.println(str);
  #endif
}

void PRINTF(const char *format, ...) {
  #ifndef LOW_POWER_MODE
  char loc_buf[64];
  char * temp = loc_buf;
  va_list arg;
  va_list copy;
  va_start(arg, format);
  va_copy(copy, arg);
  int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
  va_end(copy);
  if(len < 0) {
      va_end(arg);
  }
  if(len >= (int)sizeof(loc_buf)){  // comparation of same sign type for the compiler
      temp = (char*) malloc(len+1);
      if(temp == NULL) {
          va_end(arg);
      }
      len = vsnprintf(temp, len+1, format, arg);
  }
  va_end(arg);
  Serial.print(temp);
  if(temp != loc_buf){
      free(temp);
  }
  #endif
}

// ESPNow callback when data is sent
void ESPNow_OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  espNow.onDataSent(mac_addr, status);
}

void publishLogContent() {
  std::string jsonStr = persistentLog.readLogFileAsJsonPretty();
  espNow.sendMessage(std::string(jsonStr), LOG);
  PRINTF("Log content: %s\n", jsonStr.c_str());
}

void serialInit() {
  Serial.begin(115200);
  delay(100);
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

void resetDeepSleepTimer() {
  deepSleepTimer = DEEP_SLEEP_TIMEOUT;
}
void resetSleepTimers() {
  resetDeepSleepTimer();
  #ifdef DISPLAY_ENABLED
  display.resetDisplaySleepTimer();
  #endif
}


void initDeepSleep() {
  ESP_LOGI(LOG_TAG_MAIN, "Initiating deep sleep");
  ESP_LOGI(LOG_TAG_MAIN, "Will wakeup after %d seconds", DEEP_SLEEP_WAKEUP);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);
  delay(200);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_WAKEUP * 1000 * 1000);
  esp_deep_sleep_start();
}
void goToSleep() {
  PRINTLN("Initiating deep sleep in 6 seconds");
  espDelay(6000);
  #ifdef DISPLAY_ENABLED
  display.turnOffDisplay();
  #endif
  initDeepSleep();
}

void logBootCount() {
   //Increment boot number and print it every reboot
  ++bootCount;
  ESP_LOGI(LOG_TAG_MAIN, "Boot number: %i", bootCount);
}
void logWakeupReason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : ESP_LOGI(LOG_TAG_MAIN, "Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : ESP_LOGI(LOG_TAG_MAIN, "Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : {
      ESP_LOGI(LOG_TAG_MAIN, "Wakeup caused by timer");
      #ifdef DISPLAY_ENABLED
      display.turnOffDisplay();
      #endif
      break;
    }
    case ESP_SLEEP_WAKEUP_TOUCHPAD : ESP_LOGI(LOG_TAG_MAIN, "Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : ESP_LOGI(LOG_TAG_MAIN, "Wakeup caused by ULP program"); break;
    default : ESP_LOGI(LOG_TAG_MAIN, "Wakeup was not caused by deep sleep: %s", String(wakeup_reason)); break;
  }
}

void deep_sleep_task(void *args) {
  while(true) {
    ESP_LOGI(LOG_TAG_MAIN, "Deep sleep timer: %d", deepSleepTimer);

    if (deepSleepTimer == 0) {
      resetDeepSleepTimer();
      initDeepSleep();
    } else {
      deepSleepTimer--;
    }
    taskDelay(1000);
  }
}

void createDeepSleepTask() {
  if (deepSleepTaskHandle != NULL) {
    ESP_LOGI(LOG_TAG_MAIN, "createDeepSleepTask(): deepSleepTask already created");
    return;
  }
  ESP_LOGI(LOG_TAG_MAIN, "Creating deepSleepTask task ");
  xTaskCreate(deep_sleep_task, "deep_sleep_task", 10000, NULL, tskIDLE_PRIORITY, &deepSleepTaskHandle);
}

#ifdef NTP_TIME_ENABLED
void updateTimeInfo(const char* timeString) {
  if (strcmp(myTimeInfo.lastTime, timeString) != 0) {
    strcpy(myTimeInfo.prevTime, myTimeInfo.lastTime);
    myTimeInfo.timeChanged = true;
  } else {
    myTimeInfo.timeChanged = false;
  }
  strcpy(myTimeInfo.lastTime, timeString);
  // PRINTF("\nmyTimeInfo.lastTime: %s", myTimeInfo.lastTime);
  // PRINTF("\ntimeString: %s", timeString);
  // PRINTF("\ntimeChanged: %s", myTimeInfo.timeChanged ? "true" : "false");
}
void updateTimeTask(char* timeString, int length) {
  ntpTime.getTimeString(timeString, length);
  updateTimeInfo(timeString);
}
void update_time_task(void *arg) {
  int length = TIME_STRING_LENGTH * sizeof(char);
  char timeString[length];
  while(true) {
    updateTimeTask(timeString, length);
    taskDelay(1000);
  }
}
void createTimeTask() {
  xTaskCreate(update_time_task, "update_time_task", 2048, NULL, tskIDLE_PRIORITY, &updateTimeTaskHandle);
}
#endif

void publishWaterLevelInfo(int waterLevel) {
  char waterLevelBuff[100];
  snprintf(waterLevelBuff, 100, "{\"idx\": %d, \"nvalue\": %d}", DOMOTICZ_WATER_LEVEL_DEVICE_ID, waterLevel);
  espNow.sendMessage(std::string(waterLevelBuff), SENSOR_INFO);
}
void printWaterLevelInfo() {
  if (waterLevelTaskHandle != NULL && eTaskGetState(waterLevelTaskHandle) == eSuspended) {
    ESP_LOGI(LOG_TAG_MAIN, "printWaterLevelInfo(): task is suspended");
    return;
  }
  if (!myWaterLevelInfo.enableDisplayInfo) return;

  boolean updateValue = myWaterLevelInfo.valueOnDisplay == -1 || (myWaterLevelInfo.lastValue != myWaterLevelInfo.valueOnDisplay);
  if (updateValue) {
    int waterLevel = myWaterLevelInfo.lastValue;
    #ifdef DISPLAY_ENABLED
    display.showWaterLevel(waterLevel);
    #endif
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
void waterLevelTask() {
  int waterLevel = digitalRead(SENSOR_PIN);

    ESP_LOGI(LOG_TAG_MAIN, "Water Sensor Level: %d", waterLevel);

    updateWaterLevelInfo(waterLevel);
    publishWaterLevelInfo(waterLevel);
}
void water_level_task(void *arg) {
  while(true) {
    waterLevelTask();
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
    ESP_LOGI(LOG_TAG_MAIN, "printBatteryInfo(): task is suspended");
    return;
  }
  if (!myBatteryInfo.enableDisplayInfo) {
    return;
  }
  boolean updateVoltage = myBatteryInfo.voltageOnDisplay == -1 || (myBatteryInfo.lastVoltage != myBatteryInfo.voltageOnDisplay);
  boolean updateCharge = updateVoltage || myBatteryInfo.chargeOnDisplay == -1 || (myBatteryInfo.lastCharge != myBatteryInfo.chargeOnDisplay);
  boolean isCharging = myBatteryInfo.lastVoltage >= MIN_USB_VOL;

  #ifdef DISPLAY_ENABLED
  display.showBatteryInfo(updateCharge, myBatteryInfo.lastCharge, isCharging, updateVoltage, myBatteryInfo.lastVoltage);
  #endif

  if (updateCharge) {
    myBatteryInfo.chargeOnDisplay = myBatteryInfo.lastCharge;
  }

  if (updateVoltage) {
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
  // PRINTF("\nmyBatteryInfo.prevCharge %d", myBatteryInfo.prevCharge);
  // PRINTF("\nmyBatteryInfo.lastVoltage %f", myBatteryInfo.lastVoltage);
  // PRINTF("\nmyBatteryInfo.prevVoltage %f", myBatteryInfo.prevVoltage);
  // PRINTF("\nmyBatteryInfo.chargeChanged %s", myBatteryInfo.chargeChanged ? "true" : "false");
  // PRINTF("\nmyBatteryInfo.voltageChanged %s", myBatteryInfo.voltageChanged ? "true" : "false");
}
void batteryInfoTask() {
  int batteryChargeLevel = battery.getBatteryChargeLevel();
  double batteryVoltage = battery.getBatteryVolts();

  ESP_LOGI(LOG_TAG_MAIN, "Volts: %.2f", batteryVoltage);
  ESP_LOGI(LOG_TAG_MAIN, "Charge level: %d", batteryChargeLevel);
  ESP_LOGI(LOG_TAG_MAIN, "Charge level (using the reference table): %d", battery.getBatteryChargeLevel(true));

  updateBatteryInfo(batteryChargeLevel, batteryVoltage);
  publishBatteryInfo(batteryChargeLevel, batteryVoltage);
}
void battery_info_task(void *arg) {
  while(true) {
    batteryInfoTask();
    // PRINTF("\nbattery_info_task() - Free Stack Space: %d\n", uxTaskGetStackHighWaterMark(NULL));
    taskDelay(BATTERY_INFO_UPDATE_INTERVAL * 1000);
  }
}
void suspendBatteryInfoTask() {
  if (batteryInfoTaskHandle == NULL) {
    ESP_LOGI(LOG_TAG_MAIN, "suspendBatteryInfoTask(): batteryInfoTask not yet created");
    return;
  }
  ESP_LOGI(LOG_TAG_MAIN, "Suspending batteryInfo task");
  vTaskSuspend(batteryInfoTaskHandle);
}
void createBatteryInfoTask() {
  if (batteryInfoTaskHandle != NULL) {
    ESP_LOGI(LOG_TAG_MAIN, "createBatteryInfoTask(): batteryInfoTask already created");
    return;
  }
  ESP_LOGI(LOG_TAG_MAIN, "Creating batteryInfo task ");
  xTaskCreate(battery_info_task, "battery_info_task", 10000, NULL, tskIDLE_PRIORITY, &batteryInfoTaskHandle);
}
void resumeBatteryInfoTask() {
  if (batteryInfoTaskHandle == NULL) {
    createBatteryInfoTask();
  } else {
    ESP_LOGI(LOG_TAG_MAIN, "Resuming batteryInfo task");
    vTaskResume(batteryInfoTaskHandle);
  }
}

void printTime() {
  if (myTimeInfo.timeChanged && strcmp(myTimeInfo.timeOnDisplay, myTimeInfo.lastTime) != 0) {
    #ifdef DISPLAY_ENABLED
    display.showTime(myTimeInfo.lastTime);
    #endif
  }
  strcpy(myTimeInfo.timeOnDisplay, myTimeInfo.lastTime);
}

#ifdef DISPLAY_ENABLED
void update_display_task(void *arg) {
  while(true) {
    if (NTP_TIME_ENABLED) {
      printTime();
    }
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
        // PRINTLN("Active menu has no value to display");
    }
    taskDelay(500);
  }
}
#endif

void pinoutInit() {
  pinMode(ADC_EN, OUTPUT);
  digitalWrite(ADC_EN, HIGH);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
}

void wifi_scan()
{
  #ifdef DISPLAY_ENABLED
  display.showScanningWifi();
  #endif

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int16_t n = WiFi.scanNetworks();

  char *networksFound[512];

  for (int i = 0; i < n; ++i) {
    networksFound[i] = (char *)malloc(512 * sizeof(char));
    sprintf(wifiNetworksBuff,
            "[%d]:%s(%d)",
            i + 1,
            WiFi.SSID(i).c_str(),
            WiFi.RSSI(i));
    strcpy(networksFound[i], wifiNetworksBuff);        
  }
  
  #ifdef DISPLAY_ENABLED
  display.showWifiScanned(networksFound, n);
  #endif
  // WiFi.mode(WIFI_OFF);

  for (int i = 0; i < n; ++i) {
     free(networksFound[i]);    
  }
}

void changeMenuOption(MENUS menuOption) {
    #ifdef DISPLAY_ENABLED
    display.clearDisplayDetailArea();
    #endif
    switch (menuOption) {
    case INSTRUCTIONS:
        #ifdef DISPLAY_ENABLED
        display.showInstructions();
        #endif
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

void connectWifi() {
  PRINT("Connecting to WIFI ");
  PRINTLN(myConfig.wifiSSID);

  #ifdef DISPLAY_ENABLED
  display.showConnectingWifi(myConfig.wifiSSID);
  #endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(myConfig.wifiSSID, myConfig.wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
  }
  PRINTLN("WIFI connected.");

  #ifdef DISPLAY_ENABLED
  display.showWifiConnected(myConfig.wifiSSID, WiFi.localIP().toString().c_str());
  #endif
}

boolean validateLongClick(Button2 &b) {
  unsigned int time = b.wasPressedFor();
  boolean validTime = time >= MINIMUM_TIME_LONG_CLICK;

  if (!validTime) {
    ESP_LOGI(LOG_TAG_MAIN, "Invalid longClick time: %dms", time);
    return false;
  }
  return true;
}
void button_init()
{
  leftButton.setLongClickHandler([](Button2 & b) {
    #ifdef DISPLAY_ENABLED
    display.wakeUpDisplay();
    #endif
    resetSleepTimers();
    if (!validateLongClick(b)) return;
    ESP_LOGI(LOG_TAG_MAIN, "Left button long click");
    ESP_LOGI(LOG_TAG_MAIN, "Go to Scan WIFI...");
    changeMenuOption(WIFI_SCAN);
    wifi_scan();
  });
  leftButton.setClickHandler([](Button2 & b) {
    #ifdef DISPLAY_ENABLED
    display.wakeUpDisplay();
    #endif
    resetSleepTimers();
    ESP_LOGI(LOG_TAG_MAIN, "Left button press");
    ESP_LOGI(LOG_TAG_MAIN, "Go to Water Level info..");
    changeMenuOption(WATER_LEVEL);
  });
  leftButton.setDoubleClickHandler([](Button2 & b) {
    ESP_LOGI(LOG_TAG_MAIN, "Truncating log file");
    persistentLog.truncateLogFile();
  });

  rightButton.setLongClickHandler([](Button2 & b) {
    #ifdef DISPLAY_ENABLED
    display.wakeUpDisplay();
    #endif
    resetSleepTimers();
    if (!validateLongClick(b)) return;
    ESP_LOGI(LOG_TAG_MAIN, "Right button long click");
    changeMenuOption(DEEP_SLEEP);
    #ifdef DISPLAY_ENABLED
    display.showGoingToDeepSleep();
    #endif
    goToSleep();
  });
  rightButton.setClickHandler([](Button2 & b) {
    #ifdef DISPLAY_ENABLED
    display.wakeUpDisplay();
    #endif
    resetSleepTimers();
    ESP_LOGI(LOG_TAG_MAIN, "Right button click");
    ESP_LOGI(LOG_TAG_MAIN, "Go to Battery info..");
    changeMenuOption(BATTERY_INFO);
  });
  rightButton.setDoubleClickHandler([](Button2 & b) {
    ESP_LOGI(LOG_TAG_MAIN, "Publishing log file");
    publishLogContent();
  });
}
void button_loop()
{
  rightButton.loop();
  leftButton.loop();
}

void onDisplayWakeUp() {
  changeMenuOption(INSTRUCTIONS);
}

void loadAppConfig() {
  myAppConfig.loadConfig();
}

int redirectToLittleFS(const char *szFormat, va_list args) {
  return persistentLog.log(szFormat, args);
}

bool logFlushHandler(const char *buffer, int n) {
  return persistentLog.flushHandler(buffer, n);
}

void logInit() {
  persistentLog.flushCallback = &logFlushHandler;
  esp_log_set_vprintf(&redirectToLittleFS);
  esp_log_level_set("*", LOG_LEVEL);
}

void setup() {
  serialInit();
  pinoutInit();
  logInit();
  #ifdef DISPLAY_ENABLED
    display.init();
  #endif

  logBootCount();
  logWakeupReason();

  loadAppConfig();

  #ifdef DISPLAY_ENABLED
    changeMenuOption(INSTRUCTIONS);
    button_init();
  #endif

  espNow.init(myConfig.espNowGatewayMacAddress, myConfig.wifiSSID);

  #ifndef LOW_POWER_MODE
  #ifdef NTP_TIME_ENABLED
    createTimeTask();
  #endif
  createWaterLevelTask();
  createBatteryInfoTask();
  createDeepSleepTask();
  #else
    waterLevelTask();
    batteryInfoTask();
    initDeepSleep();
  #endif
}

void loop() {
  #ifdef DISPLAY_ENABLED
  button_loop();
  #endif
}

#ifdef DISPLAY_ENABLED
void display_sleep_task(void *args) {
  while(true) {
    ESP_LOGI(LOG_TAG_MAIN, "Display sleep timer: %d", display.getDisplaySleepTimer());
    if (display.getDisplaySleepTimer() == 0) {
      display.turnOffDisplay();
    } else {
      display.displaySleepTimerTick();
    }
    taskDelay(1000);
  }
}
#endif

