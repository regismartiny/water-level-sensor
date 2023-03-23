#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif
#define CORE_DEBUG_LEVEL 3
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <Arduino.h>
#define LOG_FILENAME "/LOGS.txt"
#define LOG_TAG "MAIN"
#define LOGE(a, ...) Log::logE(a, ##__VA_ARGS__)
#define LOGI(a, ...) Log::logI(a, ##__VA_ARGS__)
#define LOG_BUFFER_SIZE 512
#define LOG_JSON_BUFFER_SIZE 1024

static char log_print_buffer[LOG_BUFFER_SIZE];
static char log_read_buffer[LOG_BUFFER_SIZE];
static char log_write_buffer[LOG_BUFFER_SIZE];
static char log_json_buffer[LOG_JSON_BUFFER_SIZE];

class Log {
   public:
      Log();
      ~Log();
      static void logE(const char* format, ...);
      static void logI(const char* format, ...);
      static char* readLogFile();
      static char* readLogFileAsJsonPretty();
      static void truncateLogFile();
   private:
      static void SPIFFSInit();
      static void logInit();
      static void log(const char* format, esp_log_level_t logLevel, va_list args);
      static void saveLog(char* log_print_buffer, int size);
};