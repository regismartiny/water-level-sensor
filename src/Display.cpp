#include "Display.h"

Display::Display(TaskFunction_t displaySleepTaskCallback, TaskFunction_t updateDisplayTaskCallback, void (*onDisplayWakeUpCallBack)(void)) {
    mDisplaySleepTaskCallback = displaySleepTaskCallback;
    mUpdateDisplayTaskCallback = updateDisplayTaskCallback;
    mOnDisplayWakeUpCallBack = (void(*)())onDisplayWakeUpCallBack;
}

void Display::init() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setSwapBytes(true);
    createDisplaySleepTask(mDisplaySleepTaskCallback);
    createUpdateDisplayTask(mUpdateDisplayTaskCallback);
    initiated = true;
}

void Display::clearDisplayDetailArea() {
    if (!initiated) return;

    tft.setTextDatum(TL_DATUM);
    tft.fillRect(0, 20, 135, 240, TFT_BLACK);
}

void Display::resetDisplaySleepTimer() {
    if (!initiated) return;

    displaySleepTimer = DISPLAY_SLEEP_TIMEOUT;
}

boolean Display::isDisplayActive() {
    if (!initiated) return false;

    int r = digitalRead(TFT_BL);
    return r == 1;
}

void Display::turnOffDisplay() {
    if (!initiated) return;
    if (!isDisplayActive()) return;

    tft.fillScreen(TFT_BLACK);
    tft.flush();
    digitalWrite(TFT_BL, LOW);
    tft.writecommand(TFT_DISPOFF);
    tft.writecommand(TFT_SLPIN);
}

void Display::wakeUpDisplay() {
    if (!initiated) return;
    if (isDisplayActive()) return;

    tft.writecommand(TFT_DISPON);
    init();
    digitalWrite(TFT_BL, HIGH);
    resetDisplaySleepTimer();
    delay(500);
    mOnDisplayWakeUpCallBack();
}

void Display::showInstructions() {
    if (!initiated) return;

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

void Display::showConnectingWifi(char *wifiSSID) {
    if (!initiated) return;

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Connecting to " + String(wifiSSID), tft.width() / 2, tft.height() / 2);
}

void Display::showWifiConnected(char *wifiSSID, const char *localIP) {
    if (!initiated) return;

    tft.setTextDatum(MC_DATUM);
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Connected to " + String(wifiSSID), tft.width() / 2, tft.height() / 2);
    tft.drawString("IP: " + String(localIP), tft.width() / 2, tft.height() / 2 + 50);
    Serial.println(localIP);
}

void Display::showWaterLevel(int waterLevel) {
    if (!initiated) return;

    const char* waterLevelStr = waterLevel == 0 ? "OK" : "LOW";
    clearDisplayDetailArea();
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("Water Level is ", tft.width() / 2, tft.height() / 2, 2);
    tft.setTextColor(waterLevel == 0 ? TFT_GREEN : TFT_RED);
    tft.drawString(String(waterLevelStr), tft.width() / 2, tft.height() / 2 + 40, 4);
}

void Display::showTime(char *time) {
    if (!initiated) return;

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(0, 0, 120, 30, TFT_BLACK);
    // tft.drawRect(0, 0, 120, 10, TFT_YELLOW);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(time, 0, 0, 2);
}

void Display::showScanningWifi() {
    if (!initiated) return;

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString("Scan Network", tft.width() / 2, tft.height() / 2, 2);
}

void Display::showWifiScanned(char* networksFoundStr[], int networksFoundCount) {
    if (!initiated) return;

    tft.setTextDatum(MC_DATUM);
    tft.fillScreen(TFT_BLACK);
    if (networksFoundCount == 0) {
        tft.drawString("no networks found", tft.width() / 2, tft.height() / 2);
    } else {
        tft.setCursor(0, 30);
        Serial.printf("Found %d net\n", networksFoundCount);
        for (int i = 0; i < networksFoundCount; ++i) {
            tft.println(networksFoundStr[i]);
        }
    }
}

void Display::showBatteryInfo(bool updateCharge, double lastCharge, boolean isCharging, bool updateVoltage, double lastVoltage) {
    if (!initiated) return;

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_RED);
    tft.drawString("Nivel de carga", 10, 30, 2);
    if (updateCharge) {
        tft.setTextColor(TFT_GREEN);
        // tft.drawRect(10, 60, 100, 30, TFT_YELLOW);
        tft.fillRect(10, 60, 100, 30, TFT_BLACK);
        if (isCharging) {
            tft.drawString("carregando...", 10, 60, 2);
        } else {
            tft.drawString(String(lastCharge) + "%", 10, 60, 4);
        }
    }

    tft.setTextColor(TFT_RED);
    tft.drawString("Voltagem", 10, 90, 2);
    if (updateVoltage) {
        tft.setTextColor(TFT_BLUE);
        // tft.drawRect(10, 115, 100, 30, TFT_YELLOW);
        tft.fillRect(10, 115, 100, 30, TFT_BLACK);
        tft.drawString(String(lastVoltage) + "V", 10, 115, 4);
    }
}

void Display::showGoingToDeepSleep() {
    if (!initiated) return;
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Press again to wake up",  tft.width() / 2, tft.height() / 2 );
}



///// PRIVATE METHODS

void Display::createDisplaySleepTask(TaskFunction_t display_sleep_task) {
    xTaskCreate(display_sleep_task, "display_sleep_task", 10000, NULL, tskIDLE_PRIORITY, &displaySleepTaskHandle);
}

void Display::createUpdateDisplayTask(TaskFunction_t update_display_task) {
    xTaskCreate(update_display_task, "update_display_task", 10000, NULL, tskIDLE_PRIORITY, &updateDisplayTaskHandle);
}
