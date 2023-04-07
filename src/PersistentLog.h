#include <Arduino.h>
#include <FS.h>
using fs::FS; //necessary for ESPLogger to compile
#include <ESPLogger.h>

#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif
#define CORE_DEBUG_LEVEL 3
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#ifdef ESP_LOGE
#undef ESP_LOGE
#endif
#ifdef ESP_LOGW
#undef ESP_LOGW
#endif
#ifdef ESP_LOGI
#undef ESP_LOGI
#endif
#ifdef ESP_LOGD
#undef ESP_LOGD
#endif
#ifdef ESP_LOGV
#undef ESP_LOGV
#endif
#define ESP_LOGE( tag, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR,   tag, format, ##__VA_ARGS__)
#define ESP_LOGW( tag, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_WARN,    tag, format, ##__VA_ARGS__)
#define ESP_LOGI( tag, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO,    tag, format, ##__VA_ARGS__)
#define ESP_LOGD( tag, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_DEBUG,   tag, format, ##__VA_ARGS__)
#define ESP_LOGV( tag, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_VERBOSE, tag, format, ##__VA_ARGS__)

#define LOG_BUFFER_SIZE 512
#define LOG_JSON_BUFFER_SIZE 1024

class PersistentLog {
   public:
      PersistentLog(const char* filePath = "/log.txt", int sizeLimit = 1024);
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