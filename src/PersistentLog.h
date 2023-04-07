#include <Arduino.h>
#include <FS.h>
using fs::FS; //necessary for ESPLogger to compile
#include <ESPLogger.h>

#define LOG_BUFFER_SIZE 512
#define LOG_JSON_BUFFER_SIZE 1024

class PersistentLog {
   public:
      PersistentLog(const char* filePath = "/log.txt", int sizeLimit = 10240);
      ~PersistentLog();
      void init();
      char* readLogFile();
      std::string readLogFileAsJsonPretty();
      void truncateLogFile();
      int log(const char* format, va_list args);
      bool flushHandler(const char *buffer, int n);
      ESPLogger::CallbackFlush flushCallback;
   private:
      const char* filePath;
      int sizeLimit; //bytes
      static void LittleFSInit();
      static void createLogFileIfNotExists(const char* path);
      void saveLog(char* msg, int size);
      char log_print_buffer[LOG_BUFFER_SIZE];
      char log_read_buffer[LOG_BUFFER_SIZE];
      char log_write_buffer[LOG_BUFFER_SIZE];
      char log_json_buffer[LOG_JSON_BUFFER_SIZE];
      ESPLogger *logger;
};