#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif
#define CORE_DEBUG_LEVEL 3
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <Arduino.h>
#define LOG_DIRECTORY "/logs"
#define LOG_FILEPATH "/logs/log.txt"
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
      static void init();
      static char* readLogFile();
      static char* readLogFileAsJsonPretty();
      static void truncateLogFile();
      static int log(const char* format, va_list args);
   private:
      static void LittleFSInit();
      static void createDirIfNotExists(const char * path);
      static void createLogFileIfNotExists(const char * path);
      static void saveLog(char* log_print_buffer, int size);
};