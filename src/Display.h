#include <Arduino.h>
#include <TFT_eSPI.h>

#define DISPLAY_SLEEP_TIMEOUT       10 //seconds without interaction to turn off display
#define DEEP_SLEEP_WAKEUP           1800 //seconds of deep sleeping for device to wake up
#define BUTTON_RIGHT                35
#define BUTTON_LEFT                 0

enum MENUS { INSTRUCTIONS, WATER_LEVEL, WIFI_SCAN, BATTERY_INFO, DEEP_SLEEP };
struct {
  MENUS activeMenu = INSTRUCTIONS;
} myMenuInfo;

void display_sleep_task(void *args);

class Display {
    public:
        Display(TaskFunction_t displaySleepTaskCallback, TaskFunction_t updateDisplayTaskCallback, void (*onDisplayWakeUpCallBack)(void));
        void init();
        void clearDisplayDetailArea();
        boolean isDisplayActive();
        void turnOffDisplay();
        void wakeUpDisplay();
        void resetDisplaySleepTimer();
        void showInstructions();
        void showConnectingWifi(char *wifiSSID);
        void showWifiConnected(char *wifiSSID, const char *localIP);
        void showWaterLevel(int waterLevel);
        void showScanningWifi();
        void showWifiScanned(char* networksFoundStr[], int networksFoundCount);
        void showBatteryInfo(bool updateCharge, double lastCharge, boolean isCharging, bool updateVoltage, double lastVoltage);
        void showGoingToDeepSleep();
        void changeMenuOption(MENUS menuOption);
        void showTime(char *time);
        int getDisplaySleepTimer() { return displaySleepTimer; };
        void displaySleepTimerTick() { displaySleepTimer--; };
    private:
        boolean initiated;
        TFT_eSPI tft = TFT_eSPI();
        int displaySleepTimer = DISPLAY_SLEEP_TIMEOUT;
        TaskHandle_t updateDisplayTaskHandle;
        TaskHandle_t displaySleepTaskHandle;
        TaskFunction_t mDisplaySleepTaskCallback;
        TaskFunction_t mUpdateDisplayTaskCallback;
        void (*mOnDisplayWakeUpCallBack)(void);
        void createDisplaySleepTask(TaskFunction_t display_sleep_task);
        void createUpdateDisplayTask(TaskFunction_t update_display_task);
};