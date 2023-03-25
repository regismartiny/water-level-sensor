#include "Log.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

char EMPTY_STRING[1] = "";

Log::Log() {
   SPIFFSInit();
   logInit();
}

Log::~Log() {
   Serial.println("Log destructor called");
}

void Log::SPIFFSInit() {
   if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
   }
}

void Log::logInit() {
   Serial.println("Log initiated");
}

void Log::saveLog(char* msg, int size) {
   // Serial.println("saveLog()");
   if(size >= 0) {
      if(!SPIFFS.exists(LOG_FILENAME)) {
         File writeLog = SPIFFS.open(LOG_FILENAME, FILE_WRITE, true);
         if(!writeLog) {
            Serial.println("Couldn't open log file");
         }
         delay(50);
         writeLog.close();
      }
      
      File spiffsLogFile = SPIFFS.open(LOG_FILENAME, FILE_APPEND);
      //debug output
      // Serial.printf("[Writing to SPIFFS] %d, %s\n", size, msg);
      spiffsLogFile.write((uint8_t*) msg, (size_t) size);
      spiffsLogFile.write((uint8_t*) "\n", (size_t) 1);
      //to be safe in case of crashes: flush the output
      spiffsLogFile.flush();
      spiffsLogFile.close();
   }
}

void Log::log(const char* format, esp_log_level_t logLevel, va_list args) {
   int ret = vsnprintf(log_print_buffer, sizeof(log_print_buffer), format, args);
   switch(logLevel) {
      case ESP_LOG_DEBUG:
         log_d("[%s] ", log_print_buffer, LOG_TAG);
         break;
      case ESP_LOG_INFO:
         log_i("[%s] ", log_print_buffer, LOG_TAG);
         break;
      case ESP_LOG_ERROR:
      default:
         log_e("[%s] ", log_print_buffer, LOG_TAG);
         break;
   }
   saveLog(log_print_buffer, ret);
}

void Log::logE(const char* format, ...) {
   va_list args;
   va_start(args, format);
   va_end(args);

   log(format, ESP_LOG_DEBUG, args);
}

void Log::logI(const char* format, ...) {
   va_list args;
   va_start(args, format);
   va_end(args);

   log(format, ESP_LOG_INFO, args);
}

char* Log::readLogFile() {
   // Serial.printf("readLogFile()");
   File logFile = SPIFFS.open(LOG_FILENAME);
   if(!logFile){
      // Serial.println("Failed to open log file for reading");
      LOGE("Failed to open log file for reading");
      return EMPTY_STRING;
   }

   unsigned int fileSize = logFile.size();
   LOGI("Log file size: %d bytes\n", fileSize);
   //read only last n characters, n is LOG_BUFFER_SIZE
   int bytesToRead = fileSize > LOG_BUFFER_SIZE ? LOG_BUFFER_SIZE : fileSize;
   int seekPos = fileSize <= LOG_BUFFER_SIZE ? 0 : fileSize - LOG_BUFFER_SIZE;
   if (seekPos > 0) {
      logFile.seek(seekPos);
   }
   logFile.read((uint8_t*)log_read_buffer, bytesToRead);
   log_read_buffer[bytesToRead - 1] = '\0'; //end of string

   logFile.close();
   return log_read_buffer;
}

char* Log::readLogFileAsJsonPretty() {
   char* log = readLogFile();
   StaticJsonDocument<LOG_JSON_BUFFER_SIZE> json_doc;
   json_doc["content"] = log;
   int bytesWritten = serializeJsonPretty(json_doc, log_json_buffer);
   return log_json_buffer;
}

void Log::truncateLogFile() {
   SPIFFS.remove(LOG_FILENAME);
}