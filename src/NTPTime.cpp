#include "NTPTime.h"
#include <ESP32Time.h>
#include "Log.h"

NTPTime::NTPTime() {
   this->rtc = ESP32Time(this->gmtOffset_sec);
}

NTPTime::NTPTime(long gmtOffsetSec, int daylightOffsetSec, const char* ntpServer) {
   NTPTime();
   this->gmtOffset_sec = gmtOffset_sec;
   this->daylightOffset_sec = daylightOffset_sec;
   this->ntpServer = ntpServer;
}

NTPTime::~NTPTime() {
  delete &rtc;
}

void NTPTime::setTime() {
  /*---------set Time with NTP---------------*/
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)){
    rtc.setTimeStruct(timeinfo);
    // "uouehra"[timeinfo.tm_wday]
    char timeStr[30];
    getTimeStringExpanded(timeStr, 30);
    LOGI("Time received from NTP server: %s", timeStr);
  } else {
    LOGE("Error getting time from NTP server");
  }
}

void NTPTime::getTimeString(char* outStr, int length) {
  struct tm *t_st = getCurrentLocalTime();
  const char *formatStr = "%02d/%02d/%02d %02d:%02d:%02d";
  snprintf(outStr, length, formatStr, t_st->tm_mday, 1 + t_st->tm_mon, 
    abs(1900 + t_st->tm_year - 2000), t_st->tm_hour, t_st->tm_min, t_st->tm_sec);
}

void NTPTime::getTimeStringExpanded(char* outStr, int length) {
  struct tm *t_st = getCurrentLocalTime();
  const char *formatStr = "%s, %02d/%02d/%02d %02d:%02d:%02d";
  snprintf(outStr, length, formatStr, daysOfWeek[t_st->tm_wday], t_st->tm_mday, 1 + t_st->tm_mon, 
    abs(1900 + t_st->tm_year - 2000), t_st->tm_hour, t_st->tm_min, t_st->tm_sec);
}

tm * NTPTime::getCurrentLocalTime() {
  time_t t = time(NULL);
  struct tm *t_st;
  return localtime(&t);
}