#include <ESP32Time.h>

class NTPTime {
   public:
      NTPTime(long gmtOffsetSec, int daylightOffsetSec, const char* ntpServer);
      NTPTime();
      ~NTPTime();
      void setTime(); //Needs to be called once, as soon as internet connection is available
      static void getTimeString(char* outStr, int length);
   private:
      long gmtOffset_sec = -10800; // offset in seconds GMT-3
      int daylightOffset_sec = 0;
      const char* ntpServer = "br.pool.ntp.org";
      ESP32Time rtc;
};