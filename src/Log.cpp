#include "Log.h"
#include <ArduinoJson.h>
#include <esp_log.h>
#define USE_LittleFS

#include <FS.h>
#ifdef USE_LittleFS
  #define SPIFFS LITTLEFS
  #include <LITTLEFS.h> 
  #define FORMAT_LITTLEFS_IF_FAILED true
#else
  #include <SPIFFS.h>
#endif

#define FORMAT_LITTLEFS_IF_FAILED true

char EMPTY_STRING[1] = "";

Log::Log() {
   ESP_LOGI("LOG", "Log constructor called");
}

Log::~Log() {
   ESP_LOGI("LOG", "Log destructor called");
}

void Log::LittleFSInit() {
   if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
      ESP_LOGE("LOG", "LITTLEFS Mount Failed");
      return;
   }
   delay(50);
}

void Log::createDirIfNotExists(const char * path){
   if(LittleFS.exists(path)) return;
   ESP_LOGI("LOG", "Creating Dir: %s\n", path);
   if(LittleFS.mkdir(path)){
      ESP_LOGI("LOG", "Dir created");
   } else {
      ESP_LOGI("LOG", "Dir creation failed");
   }
}

void Log::createLogFileIfNotExists(const char * path){
   if(LittleFS.exists(path)) return;
   ESP_LOGI("LOG", "Log file not found. Creating file");
   File writeLog = LittleFS.open(LOG_FILEPATH, FILE_WRITE);
   if(!writeLog) {
      ESP_LOGE("LOG", "Log file creation failed");
      return;
   }
   delay(50);
   writeLog.close();
   ESP_LOGI("LOG", "Log file created");
}

void Log::init() {
   LittleFSInit();
   createDirIfNotExists(LOG_DIRECTORY);
   createLogFileIfNotExists(LOG_FILEPATH);
   ESP_LOGI("LOG", "Persistent log initiated");
}

void Log::saveLog(char* msg, int size) {
   if(size >= 0) {      
      File logFile = LittleFS.open(LOG_FILEPATH, FILE_APPEND);
      //debug output
      // Serial.printf("[Writing to LittleFS] %d, %s\n", size, msg);
      logFile.write((uint8_t*) msg, (size_t) size);
      logFile.write((uint8_t*) "\n", (size_t) 1);
      //to be safe in case of crashes: flush the output
      logFile.flush();
      logFile.close();
   }
}

int Log::log(const char* format, va_list args) {
   int ret = vsnprintf(log_print_buffer, sizeof(log_print_buffer), format, args);
   saveLog(log_print_buffer, ret);
   return ret;
}

char* Log::readLogFile() {
   // Serial.printf("readLogFile()");
   File logFile = LittleFS.open(LOG_FILEPATH);
   if(!logFile){
      ESP_LOGE("LOG", "Failed to open log file for reading");
      return EMPTY_STRING;
   }

   unsigned int fileSize = logFile.size();
   ESP_LOGI("LOG", "Log file size: %d bytes\n", fileSize);
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
   LittleFS.remove(LOG_FILEPATH);
}