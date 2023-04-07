#include "PersistentLog.h"
#include <ArduinoJson.h>
#include "ESPLogMacros.h"
#define FORMAT_LITTLEFS_IF_FAILED true

char EMPTY_STRING[1] = "";

PersistentLog::PersistentLog(const char* filePath, int sizeLimit) {
   this->filePath = filePath;
   this->sizeLimit = sizeLimit;
   Serial.println("Log constructor called");
   init();
}

PersistentLog::~PersistentLog() {
   Serial.println("Log destructor called");
}

void PersistentLog::LittleFSInit() {
   if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
      Serial.println("LITTLEFS Mount Failed");
      return;
   }
   delay(50);
}

void PersistentLog::createLogFileIfNotExists(const char* path){
   if(LittleFS.exists(path)) return;
   Serial.println("Creating log file");
   File writeLog = LittleFS.open(path, FILE_WRITE);
   if(!writeLog) {
      Serial.println("Log file creation failed");
      return;
   }
   delay(50);
   writeLog.close();
   Serial.println("Log file created");
}

bool PersistentLog::flushHandler(const char *buffer, int n) {
   int index = 0;
   // Print a record at a time
   while (index < n && strlen(&buffer[index]) > 0) {
      Serial.print("---");
      int bytePrinted = Serial.print(&buffer[index]);
      Serial.println("---");
      // +1, since '\0' is processed
      index += bytePrinted + 1;
  }
  return true;
}

void PersistentLog::init() {
   LittleFSInit();
   logger = new ESPLogger(filePath);
   // Set the space reserved to the log (in bytes)
   logger->setSizeLimit(sizeLimit);
   logger->setFlushCallback(flushCallback);
   logger->begin();
   // createLogFileIfNotExists(filePath);
   Serial.println("Persistent log initiated");
}

void PersistentLog::saveLog(char* msg, int size) {
   if(size < 0) return;
   
   bool success = logger->append(msg, true);
   if (success) {
      Serial.println("Record stored!");
   } else {
      if (logger->isFull()) {
         Serial.println("Record NOT stored! You had filled the available space: flush or reset the "
                        "log before appending another record");
      } else {
         Serial.println("Something goes wrong, record NOT stored!");
      }
   }
}

int PersistentLog::log(const char* format, va_list args) {
   int ret = vsnprintf(log_print_buffer, sizeof(log_print_buffer), format, args);
   saveLog(log_print_buffer, ret);
   return vprintf(format, args);
}

char* PersistentLog::readLogFile() {
   // Serial.printf("readLogFile()");
   File logFile = LittleFS.open(filePath);
   if(!logFile){
      Serial.println("Failed to open log file for reading");
      return EMPTY_STRING;
   }

   unsigned int fileSize = logFile.size();
   Serial.printf("Log file size: %d bytes\n", fileSize);
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

std::string PersistentLog::readLogFileAsJsonPretty() {
   char* log = readLogFile();
   StaticJsonDocument<LOG_JSON_BUFFER_SIZE> json_doc;
   json_doc["content"] = log;
   int bytesWritten = serializeJsonPretty(json_doc, log_json_buffer);
   return std::string(log_json_buffer);
}

void PersistentLog::truncateLogFile() {
   LittleFS.remove(filePath);
}