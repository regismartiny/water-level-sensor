#include "NTPTime.h"
#include <ESP32Time.h>

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
    Serial.println("Time received from NTP");
    Serial.printf("year: %d\n", 1900 + timeinfo.tm_year);
    Serial.printf("month: %d\n", 1 + timeinfo.tm_mon);
    Serial.printf("month day: %d\n", timeinfo.tm_mday);
    Serial.printf("week day: %c%c\n", "SMTWTFS"[timeinfo.tm_wday], "uouehra"[timeinfo.tm_wday]);
    Serial.printf("year day: %d\n", 1 + timeinfo.tm_yday);
    Serial.printf("hour: %d\n", timeinfo.tm_hour);
    Serial.printf("minute: %d\n", timeinfo.tm_min);
    Serial.printf("second: %d\n", timeinfo.tm_sec);
  } else {
    Serial.println("Error getting time from NTP");
  }
}

void NTPTime::getTimeString(char* outStr, int length) {
  time_t t = time(NULL);
  struct tm *t_st;
  t_st = localtime(&t);
  snprintf(outStr, length, "%02d/%02d/%02d %02d:%02d:%02d", t_st->tm_mday, 1 + t_st->tm_mon, 
  abs(1900 + t_st->tm_year - 2000), t_st->tm_hour, t_st->tm_min, t_st->tm_sec);
}